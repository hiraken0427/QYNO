'use strict';
const { STANDARD, KEYWORD_COMPLETIONS, properties } = require('./catalog');
const { TYPES, codeContext, visibleSymbols, visibleUsings, functionLabel, typeLabel } = require('./analysis');

function environment(project, line, character) {
    const model = project.model;
    const imports = model.imports.map(i => i.path);
    const usings = visibleUsings(model, line, character);
    const aliases = new Map();
    const names = new Map();
    const namespace = path => ({ name: path.split('.').at(-1), path, kind: 'namespace', documentation: `${path} 名前空間` });
    const exported = (path) => {
        if (['io', 'io.file', 'math'].includes(path)) return namespace(path);
        const builtin = STANDARD.find(s => s.path === path);
        if (builtin) return builtin;
        const modulePath = imports.filter(p => path === p || path.startsWith(p + '.')).sort((a, b) => b.length - a.length)[0];
        if (modulePath) {
            if (path === modulePath) return namespace(path);
            const name = path.slice(modulePath.length + 1);
            return project.modules.get(modulePath)?.symbols.find(s => s.scopeId === 0 && s.name === name);
        }
        if (imports.some(p => p.startsWith(path + '.'))) return { ...namespace(path), implicit: true };
        return undefined;
    };
    const resolve = (path, seen = new Set()) => {
        if (seen.has(path) || seen.size > 32) return undefined;
        seen.add(path);
        const [first, ...rest] = path.split('.');
        if (aliases.has(first)) return resolve(aliases.get(first) + (rest.length ? '.' + rest.join('.') : ''), seen);
        if (rest.length && names.get(first)?.kind === 'namespace' && names.get(first).path !== first)
            return resolve(names.get(first).path + '.' + rest.join('.'), seen);
        return exported(path) || (!rest.length ? names.get(path) : undefined);
    };
    const members = path => {
        const resolved = resolve(path);
        if (resolved?.kind !== 'namespace') return [];
        const full = resolved.path, result = new Map();
        for (const item of STANDARD) if (item.path.startsWith(full + '.') && !item.path.slice(full.length + 1).includes('.')) result.set(item.name, item);
        if (full === 'io') result.set('file', namespace('io.file'));
        for (const imported of imports) if (imported.startsWith(full + '.')) {
            const child = imported.slice(full.length + 1).split('.')[0];
            result.set(child, namespace(full + '.' + child));
        }
        for (const symbol of project.modules.get(full)?.symbols || []) if (symbol.scopeId === 0) result.set(symbol.name, symbol);
        return [...result.values()];
    };
    for (const symbol of STANDARD) if (!symbol.path.includes('.') || ['io.print', 'io.input'].includes(symbol.path)) names.set(symbol.name, symbol);
    for (const root of ['io', 'math', ...imports.map(p => p.split('.')[0])]) names.set(root, namespace(root));
    // Resolve module aliases first; fixed-point iteration handles global forward references.
    for (let pass = 0; pass <= usings.length; pass++) for (const u of usings) if (u.alias) {
        const target = resolve(u.path);
        if (target?.kind === 'namespace') aliases.set(u.alias, target.path);
    }
    for (const [name, path] of aliases) names.set(name, { ...namespace(path), name });
    for (const u of usings) if (!u.alias) {
        const target = resolve(u.path);
        if (target?.kind === 'namespace' && !target.implicit) for (const item of members(target.path)) names.set(item.name, item);
        else if (target) names.set(u.path.split('.').at(-1), target);
    }
    for (const symbol of visibleSymbols(model, line, character)) names.set(symbol.name, symbol);
    return { names, resolve, members };
}

function snippetCall(symbol, name = symbol.name) {
    // QYNO declarations put dimensions after names, but call arguments use names alone.
    return `${name}(${symbol.params.map((p, i) => `${p.ref ? 'ref ' : ''}\${${i + 1}:${p.placeholder || p.default || p.name}}`).join(', ')})`;
}
function completion(symbol, label = symbol.name) {
    return { label, kind: symbol.kind, detail: symbol.kind === 'function' ? functionLabel(symbol, label) : symbol.type ? typeLabel({ ...symbol, name: label }) : symbol.path,
        documentation: symbol.documentation || (symbol.kind === 'function' ? 'ユーザー定義関数' : symbol.kind === 'constant' ? '定数' : 'ユーザー定義の変数'),
        insertText: symbol.kind === 'function' ? snippetCall(symbol, label) : label,
        snippet: symbol.kind === 'function', symbol };
}
function receiverMembers(project, env, text) {
    const match = /([A-Za-z_]\w*)((?:\[[^\]\n]*\])*)(?:\.value)?\.$/.exec(text);
    if (!match) return [];
    const symbol = env.names.get(match[1]);
    if (!symbol || !['variable', 'constant', 'parameter'].includes(symbol.kind)) return [];
    const indexed = (match[2].match(/\[/g) || []).length;
    const rank = Math.max(0, (symbol.rank || 0) - indexed);
    const result = [];
    if (!indexed && !text.endsWith('.value.')) result.push(properties.addr, { ...properties.value, type: symbol.type });
    if (rank || ['str', 'char'].includes(symbol.type)) result.push(properties.length);
    if (rank && symbol.kind !== 'constant') result.push(properties.add, properties.remove_at, properties.clear);
    return result;
}
function complete(project, line, character) {
    const context = codeContext(project.model, line, character);
    if (!context) return [];
    const env = environment(project, line, character), text = context.text;
    const word = /[A-Za-z_0-9]*$/.exec(text)[0];
    const before = text.slice(0, text.length - word.length);
    const range = { start: character - word.length, end: character };
    let items;
    const clause = /^\s*(import|using)\s+([^\n]*)$/.exec(text);
    if (clause) {
        if (/\bas\s+\w*$/.test(clause[2])) return [];
        const part = clause[2].split(',').at(-1).trim();
        const base = part.includes('.') ? part.slice(0, part.lastIndexOf('.')) : '';
        const paths = [...new Set(['io', 'io.file', 'math', ...(project.moduleNames || []), ...project.model.imports.map(i => i.path), ...[...env.names].filter(([, s]) => s.kind === 'namespace').map(([name]) => name)])];
        const moduleItems = new Map();
        for (const path of paths) if (!base || path.startsWith(base + '.')) {
            const label = (base ? path.slice(base.length + 1) : path).split('.')[0];
            if (label) moduleItems.set(label, completion({ name: label, path: base ? base + '.' + label : label, kind: 'namespace' }));
        }
        if (clause[1] === 'using' && base) for (const member of env.members(base)) moduleItems.set(member.name, completion(member));
        items = [...moduleItems.values()];
    } else if (before.endsWith('.')) {
        const qualified = /([A-Za-z_]\w*(?:\.[A-Za-z_]\w*)*)\.$/.exec(before);
        items = (qualified ? env.members(qualified[1]) : []).map(s => completion(s));
        if (!items.length) items = receiverMembers(project, env, before).map(s => completion(s));
    } else {
        items = [...env.names].map(([label, s]) => completion(s, label));
        for (const keyword of KEYWORD_COMPLETIONS) if (!env.names.has(keyword)) items.push({ label: keyword, kind: TYPES.has(keyword) ? 'type' : 'keyword', insertText: keyword, detail: 'QYNO 0.25.2' });
    }
    // A completion immediately before '(' must not insert a second argument list.
    const next = project.model.lines[line].slice(character).match(/^\s*\(/);
    return items.filter(item => item.label.startsWith(word)).map(item => ({ ...item, range, insertText: next && item.kind === 'function' ? item.label : item.insertText, snippet: next ? false : item.snippet }));
}

function symbolAt(project, line, character) {
    const context = codeContext(project.model, line, character);
    if (!context) return undefined;
    const text = project.model.lines[line];
    const declaration = project.model.symbols.find(s => s.line === line && character >= s.start && character <= s.end);
    if (declaration) return declaration;
    let start = character, end = character;
    while (start > context.start && /[A-Za-z0-9_.]/.test(text[start - 1])) start--;
    while (end < text.length && /[A-Za-z0-9_.]/.test(text[end])) end++;
    const full = text.slice(start, end), before = text.slice(start, character);
    // Choose the component under the pointer rather than a later member.
    const after = full.slice(before.length), stop = after.indexOf('.');
    const path = stop < 0 ? full : full.slice(0, before.length + stop);
    const env = environment(project, line, character);
    return env.resolve(path) || env.names.get(path);
}

/** Find the innermost unmatched call; strings, interpolations and arrays are lexical. */
function callAt(model, line, character) {
    const context = codeContext(model, line, character);
    if (!context) return undefined;
    const ts = context.tokens.filter(t => t.start < character && t.kind !== 'comment');
    const stack = [];
    for (let i = 0; i < ts.length; i++) {
        const value = ts[i].value;
        if (value === '(') {
            let j = i - 1, path = '';
            while (j >= 0 && (ts[j].kind === 'id' || ts[j].value === '.')) path = ts[j--].value + path;
            stack.push({ kind: '(', path, argument: 0 });
        } else if (value === '[') stack.push({ kind: '[' });
        else if (value === ')' || value === ']') stack.pop();
        else if (value === ',' && stack.at(-1)?.kind === '(') stack.at(-1).argument++;
    }
    return [...stack].reverse().find(s => s.kind === '(' && s.path);
}
function signature(project, line, character) {
    const call = callAt(project.model, line, character);
    if (!call) return undefined;
    const env = environment(project, line, character);
    let symbol = env.resolve(call.path) || env.names.get(call.path);
    if (!symbol && call.path.includes('.')) {
        const last = call.path.split('.').at(-1);
        const candidate = properties[last];
        if (candidate?.kind === 'function' && receiverMembers(project, env, call.path.slice(0, -last.length)).some(s => s.name === last)) symbol = candidate;
    }
    return symbol?.kind === 'function' ? { symbol, label: functionLabel(symbol, call.path), activeParameter: Math.min(call.argument, Math.max(0, symbol.params.length - 1)) } : undefined;
}
module.exports = { environment, complete, symbolAt, callAt, signature, completion, snippetCall };
