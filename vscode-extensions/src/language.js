'use strict';
const { STANDARD, KEYWORD_COMPLETIONS, properties } = require('./catalog');
const { TYPES, codeContext, visibleSymbols, visibleUsings, functionLabel, typeLabel, scopeContains } = require('./analysis');
const { initializationContext } = require('./initialization');

function environment(project, line, character) {
    const model = project.model;
    const imports = model.imports.map(i => i.path);
    const usings = visibleUsings(model, line, character);
    const aliases = new Map();
    const names = new Map();
    const namespace = path => ({ name: path.split('.').at(-1), path, kind: 'namespace', documentation: `${path} 名前空間` });
    const exported = (path) => {
        if (['io', 'io.file', 'io.path', 'math', 'lib'].includes(path)) return namespace(path);
        const builtin = STANDARD.find(s => s.path === path);
        if (builtin) return builtin;
        const enumeration=STANDARD.find(s=>s.kind==='enum'&&path.startsWith(s.path+'.'));
        if(enumeration)return enumeration.members.find(m=>m.name===path.slice(enumeration.path.length+1));
        const modulePath = imports.filter(p => path === p || path.startsWith(p + '.')).sort((a, b) => b.length - a.length)[0];
        if (modulePath) {
            if (path === modulePath) return namespace(path);
            const name = path.slice(modulePath.length + 1);
            const [head,member]=name.split('.');
            const symbol=project.modules.get(modulePath)?.symbols.find(s => s.scopeId === 0 && s.name === head && (s.visibility!=='private'||s.uri===model.uri));
            return member?symbol?.kind==='enum'?symbol.members.find(m=>m.name===member):undefined:symbol;
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
        const container=names.get(first)||model.symbols.find(s=>s.kind==='enum'&&s.name===first&&s.scopeId===0);
        if(rest.length===1&&container?.kind==='enum')return container.members.find(m=>m.name===rest[0]);
        return exported(path) || (!rest.length ? names.get(path) : undefined);
    };
    const members = path => {
        const resolved = resolve(path);
        if(resolved?.kind==='enum')return resolved.members;
        if (resolved?.kind !== 'namespace') return [];
        const full = resolved.path, result = new Map();
        for (const item of STANDARD) if (item.path.startsWith(full + '.') && !item.path.slice(full.length + 1).includes('.')) result.set(item.name, item);
        if (full === 'io') for(const child of ['file','path']) result.set(child, namespace('io.'+child));
        for (const imported of imports) if (imported.startsWith(full + '.')) {
            const child = imported.slice(full.length + 1).split('.')[0];
            result.set(child, namespace(full + '.' + child));
        }
        for (const symbol of project.modules.get(full)?.symbols || []) if (symbol.scopeId === 0 && (symbol.visibility!=='private'||symbol.uri===model.uri)) result.set(symbol.name, symbol);
        return [...result.values()];
    };
    for (const symbol of STANDARD) if (!symbol.path.includes('.')) names.set(symbol.name, symbol);
    for (const root of ['io', 'math', 'lib', ...imports.map(p => p.split('.')[0])]) names.set(root, namespace(root));
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
    const modelType = symbol => {
        if (!symbol || symbol.rank) return undefined;
        const source = symbol.uri && symbol.uri !== model.uri ? [...project.modules.values()].find(m => m.uri === symbol.uri) : model;
        const local = source?.symbols.find(s => ['model', 'interface', 'enum'].includes(s.kind) && s.scopeId === 0 && s.name === symbol.type);
        if (local) return local;
        const found = source && source !== model ? environment({ ...project, model: source }, symbol.line || 0, source.lines[symbol.line || 0]?.length || 0).resolve(symbol.type || '') : resolve(symbol.type || '');
        return ['model', 'interface', 'enum'].includes(found?.kind) ? found : undefined;
    };
    const owner = model.scopes.filter(s => s.kind === 'model' && scopeContains(model, s.id, line, character)).at(-1)?.owner;
    const canAccess = member => member.visibility !== 'private' || member.declaringModelId === owner?.modelId;
    return { names, resolve, members, modelType, canAccess };
}

function snippetCall(symbol, name = symbol.name) {
    if (symbol.builtin && /^complex(?:_(?:64|128))?$/.test(symbol.path)) return `${name}(real=\${1:0.0}, imag=\${2:0.0})`;
    // QYNO declarations put dimensions after names, but call arguments use names alone.
    const params=symbol.builtin && /^(?:(?:unsigned )?int(?:_(?:8|16|32|64|128))?|str|complex(?:_(?:64|128))?)$/.test(symbol.path) ? symbol.params.filter(p=>!p.optional) : symbol.params;
    return `${name}(${params.map((p, i) => `${p.ref ? 'ref ' : ''}\${${i + 1}:${p.placeholder || p.default || p.name}}`).join(', ')})`;
}
function completion(symbol, label = symbol.name) {
    return { label, kind: symbol.kind, detail: ['model', 'interface', 'enum'].includes(symbol.kind) ? `${symbol.kind} ${label}` : symbol.kind === 'function' ? functionLabel(symbol, label) : symbol.type ? typeLabel({ ...symbol, name: label }) : symbol.path,
        documentation: symbol.documentation || (symbol.visibility === 'private' ? 'private: 宣言元のモデル内からのみアクセスできます' : symbol.kind === 'function' ? 'ユーザー定義関数' : symbol.kind === 'constant' ? '定数' : 'ユーザー定義の変数'),
        insertText: symbol.kind === 'function' ? snippetCall(symbol, label) : label,
        snippet: symbol.kind === 'function', symbol };
}
function receiverSymbol(env, text, includePrivate = false) {
    const match = /([A-Za-z_]\w*(?:(?:\[[^\]\n]*\])|(?:\.[A-Za-z_]\w*))*)\.?$/.exec(text);
    if (!match) return undefined;
    const parts = match[1].match(/[A-Za-z_]\w*|\[[^\]]*\]/g);
    let symbol = env.names.get(parts.shift());
    for (const part of parts) {
        if (!symbol) return undefined;
        if (part.startsWith('[')) {
            if (!symbol.rank) return undefined;
            symbol = { ...symbol, rank: symbol.rank - 1 };
        } else if (symbol.kind === 'enum') {symbol=symbol.members.find(m=>m.name===part);}
        else if (symbol.kind === 'namespace') { symbol = env.resolve(symbol.path + '.' + part); }
        else if (part === 'value') { if (env.modelType(symbol)?.kind === 'interface') return undefined; }
        else if (!symbol.rank && /^complex(?:_(?:64|128))?$/.test(symbol.type) && ['real','imag'].includes(part)) {
            symbol={...properties[part],type:symbol.type==='complex_128'?'double':'float'};
        }
        else {
            const receiverReadonly = symbol.readonly;
            symbol = env.modelType(symbol)?.members.find(m => m.name === part);
            if (symbol && !includePrivate && (!env.canAccess(symbol) || receiverReadonly && symbol.kind === 'function' && !symbol.readonly)) return undefined;
            if (symbol?.kind === 'field' && receiverReadonly) symbol = { ...symbol, readonly: true };
        }
    }
    return symbol;
}
function receiverMembers(project, env, text) {
    const symbol = receiverSymbol(env, text.slice(0, -1));
    if (!symbol || !['variable', 'constant', 'parameter', 'field'].includes(symbol.kind)) return [];
    const rank = symbol.rank || 0, result = [];
    const contract = env.modelType(symbol)?.kind === 'interface';
    if (!contract && /^[A-Za-z_]\w*\.$/.test(text.trim())) {
        if (!symbol.readonly) result.push(properties.addr);
        result.push({ ...properties.value, type: symbol.type });
    }
    const model = env.modelType(symbol);
    if (model && model.kind!=='enum') result.push(...model.members.filter(m => !m.initializer && env.canAccess(m) && (!symbol.readonly || m.kind !== 'function' || m.readonly)).map(m => m.kind === 'field' && symbol.readonly ? { ...m, readonly: true } : m));
    if (!rank && /^complex(?:_(?:64|128))?$/.test(symbol.type)) {
        const type=symbol.type==='complex_128'?'double':'float';
        result.push({...properties.real,type},{...properties.imag,type});
    }
    if (rank || ['str', 'char'].includes(symbol.type)) result.push(properties.length);
    if (rank && !symbol.readonly) result.push(properties.add, properties.remove_at, properties.clear);
    return result;
}
function enumContext(project,env,line,character,text){
    if(/^\s*case\b/.test(text)){
        if(/^\s*case\s*\(/.test(text)&&!project.model.scopes.some(s=>s.kind==='match'&&scopeContains(project.model,s.id,line,character)))return env.resolve('io.ErrorCode');
        const scope=project.model.scopes.filter(s=>s.kind==='match'&&scopeContains(project.model,s.id,line,character)).at(-1);
        const target=scope&&(env.resolve(scope.targetText)||receiverSymbol(env,scope.targetText));
        const type=env.modelType(target);return type?.kind==='enum'?type:undefined;
    }
    const expected=initializationContext(project,env,line,character,receiverSymbol,true);
    const type=env.modelType(expected);
    if(type?.kind==='enum')return type;
    const comparison=/([A-Za-z_]\w*(?:(?:\[[^\]]*\])|(?:\.[A-Za-z_]\w*))*)\s*(?:==|!=)\s*\w*$/.exec(text);
    const target=comparison&&receiverSymbol(env,comparison[1]);
    const compared=env.modelType(target);return compared?.kind==='enum'?compared:undefined;
}
function complete(project, line, character) {
    const context = codeContext(project.model, line, character);
    if (!context) return [];
    const env = environment(project, line, character), text = context.text;
    if (context.format) {
        const word=/[A-Za-z0-9]*$/.exec(text)[0];
        return [['0b','2進'],['0o','8進'],['0d','10進'],['0x','16進']].filter(([code])=>code.startsWith(word)).map(([code,label])=>({
            label:code,kind:'property',detail:label+'の整数書式',insertText:code,snippet:false,range:{start:character-word.length,end:character}}));
    }
    const word = /[A-Za-z_0-9]*$/.exec(text)[0];
    const before = text.slice(0, text.length - word.length);
    const range = { start: character - word.length, end: character };
    const init = context.start === 0 && initializationContext(project, env, line, character, receiverSymbol);
    if (init?.stage === 'key') {
        const equal = project.model.lines[line].slice(character).match(/^\s*=/);
        return init.candidates.filter(p => (!init.used.has(p.name) || p.name === init.key?.value) && p.name.startsWith(word)).map(p => ({
            ...completion(p), kind: 'property', range, insertText: p.name + (equal ? '' : '='), snippet: false,
            documentation: init.model.initializer ? `${init.model.name}.init の${p.defaultText === undefined ? '必須' : '省略可能な'}引数` : 'モデルのフィールド初期値'
        }));
    }
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
        const enumeration=enumContext(project,env,line,character,text);
        if(enumeration)for(const member of enumeration.members)if(!env.names.has(member.name))items.push(completion(member));
        for(const symbol of STANDARD) if(symbol.path.includes('.') && env.names.get(symbol.name)?.path!==symbol.path) items.push(completion(symbol,symbol.path));
        for (const keyword of KEYWORD_COMPLETIONS) if (!env.names.has(keyword)) items.push({ label: keyword, kind: TYPES.has(keyword) ? 'type' : 'keyword', insertText: keyword, detail: 'QYNO 0.27.1' });
    }
    // A completion immediately before '(' must not insert a second argument list.
    const next = project.model.lines[line].slice(character).match(/^\s*\(/);
    return items.filter(item => item.label.startsWith(word)).map(item => ({ ...item, range, insertText: next && item.kind === 'function' ? item.label : item.insertText, snippet: next ? false : item.snippet }));
}

function symbolAt(project, line, character) {
    const context = codeContext(project.model, line, character);
    if (!context || context.format) return undefined;
    const text = project.model.lines[line];
    const init = context.start === 0 && initializationContext(project, environment(project, line, character), line, character, receiverSymbol);
    if (init?.keySymbol && init.key.line === line && character >= init.key.start && character <= init.key.end) return init.keySymbol;
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
    const qualified = /((?:unsigned\s+)?[A-Za-z_]\w*(?:(?:\[[^\]\n]*\])|(?:\.[A-Za-z_]\w*))*)$/.exec(text.slice(context.start, start + path.length))?.[1] || path;
    return env.resolve(path) || env.names.get(path) || receiverSymbol(env, qualified, true) || enumContext(project,env,line,character,context.text)?.members.find(m=>m.name===path);
}

/** Find the innermost unmatched call; strings, interpolations and arrays are lexical. */
function callAt(model, line, character) {
    const context = codeContext(model, line, character);
    if (!context || context.format) return undefined;
    const ts = context.tokens.filter(t => t.start < character && t.kind !== 'comment');
    const stack = [];
    for (let i = 0; i < ts.length; i++) {
        const value = ts[i].value;
        if (value === '(') {
            let j = i - 1, path = '';
            while (j >= 0 && (ts[j].kind === 'id' || ts[j].value === '.')) path = ts[j--].value + path;
            const prefix = model.lines[line].slice(context.start, ts[i].start);
            path = /((?:unsigned\s+)?[A-Za-z_]\w*(?:(?:\[[^\]\n]*\])|(?:\.[A-Za-z_]\w*))*)$/.exec(prefix)?.[1] || path;
            stack.push({ kind: '(', path, argument: 0 });
        } else if (value === '[' || value === '{') stack.push({ kind: value });
        else if (value === ')' || value === ']' || value === '}') stack.pop();
        else if (value === ',' && stack.at(-1)?.kind === '(') { stack.at(-1).argument++; stack.at(-1).argName=undefined; }
        else if (value === '=' && stack.at(-1)?.kind === '(' && ts[i-1]?.kind==='id') stack.at(-1).argName=ts[i-1].value;
    }
    return [...stack].reverse().find(s => s.kind === '(' && s.path);
}
function signature(project, line, character) {
    const env = environment(project, line, character);
    const init = initializationContext(project, env, line, character, receiverSymbol);
    if (init?.model.initializer) {
        const symbol = init.model.initializer;
        return { symbol, label: functionLabel(symbol, `${init.model.name}.init`), activeParameter: Math.max(0, symbol.params.findIndex(p => p.name === init.key?.value)) };
    }
    const call = callAt(project.model, line, character);
    if (!call) return undefined;
    let symbol = env.resolve(call.path) || env.names.get(call.path) || receiverSymbol(env, call.path);
    if (!symbol && call.path.includes('.')) {
        const last = call.path.split('.').at(-1);
        const candidate = properties[last];
        if (candidate?.kind === 'function' && receiverMembers(project, env, call.path.slice(0, -last.length)).some(s => s.name === last)) symbol = candidate;
    }
    if(symbol?.kind==='enum')return {symbol,label:call.path+'(value: integer / str / '+symbol.name+')',activeParameter:0};
    const index=symbol?.builtin && /^(?:(?:unsigned )?int(?:_(?:8|16|32|64|128))?|str|complex(?:_(?:64|128))?)$/.test(symbol.path) && call.argName ? symbol.params.findIndex(p=>p.name===call.argName) : call.argument;
    return symbol?.kind === 'function' ? { symbol, label: functionLabel(symbol, call.path), activeParameter: Math.min(Math.max(0,index), Math.max(0, symbol.params.length - 1)) } : undefined;
}
module.exports = { environment, complete, symbolAt, callAt, signature, completion, snippetCall };
