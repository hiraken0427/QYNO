'use strict';
const { scopeContains } = require('./analysis');

/** Infer an initializer's contextual type without executing or compiling code.
 * Unknown calls/expressions deliberately produce no named-argument guesses. */
function initializationContext(project, env, line, character, receiver, valueOnly = false) {
    const model = project.model, tokens = [];
    for (let row = 0; row <= line; row++) for (const t of model.tokens[row] || [])
        if (t.kind !== 'comment' && (row < line || t.start < character)) tokens.push(t);
    const stack = []; let statement = 0;
    const pathBefore = index => {
        const t = tokens[index - 1];
        if (!t) return '';
        const text = model.lines[t.line].slice(0, t.end);
        return /([A-Za-z_]\w*(?:(?:\[[^\]\n]*\])|(?:\.[A-Za-z_]\w*))*)$/.exec(text)?.[1] || '';
    };
    const named = frame => frame?.model?.kind === 'interface' ? [] : frame?.model?.initializer?.params || frame?.model?.members.filter(m => m.kind === 'field') || [];
    const expected = index => {
        const parent = stack.at(-1);
        if (parent?.kind === '{' && parent.stage === 'value') return named(parent).find(p => p.name === parent.key?.value);
        if (parent?.kind === '[' && parent.literal) return parent.element;
        if (parent?.kind === '(' && parent.fn) return parent.fn.params[parent.argument];
        if (parent?.kind === '(' && parent.appendType) return parent.appendType;
        const start = parent ? parent.start + 1 : statement;
        const prefix = tokens.slice(start, index);
        const eq = prefix.findLastIndex(t => t.value === '=');
        if (eq >= 0) {
            const mark = prefix[eq];
            const declaration = model.symbols.filter(s => ['variable','constant','field','parameter'].includes(s.kind) && s.start >= 0 && s.line === mark.line && s.end <= mark.start).at(-1);
            // An '=' after a declaration identifier establishes its declared type.
            if (declaration) return declaration;
            const text = model.lines[mark.line].slice(0, mark.start).trim();
            return receiver(env, text);
        }
        if (prefix[0]?.value === 'return')
            return model.scopes.filter(s => s.kind === 'function' && scopeContains(model, s.id, line, character)).at(-1)?.owner;
        return undefined;
    };
    for (let i = 0; i < tokens.length; i++) {
        const t = tokens[i], parent = stack.at(-1);
        if (!parent && i && t.line !== tokens[i - 1].line) statement = i;
        if (t.value === '{') {
            const type = expected(i), target = type && !type.rank ? env.modelType(type) : undefined;
            stack.push({ kind: '{', start: i, model: target, stage: 'key', used: new Set(), key: undefined });
        } else if (t.value === '[') {
            const type = expected(i), prev = tokens[i - 1]?.value;
            const literal = type?.rank > 0 && (['=', '[', '(', ',', 'return'].includes(prev) || !prev);
            stack.push({ kind: '[', start: i, literal, element: literal ? { ...type, rank: type.rank - 1 } : undefined });
        } else if (t.value === '(') {
            const path = pathBefore(i), fn = env.resolve(path) || env.names.get(path) || receiver(env, path);
            const appended = path.endsWith('.append') ? receiver(env, path.slice(0, -7)) : undefined;
            stack.push({ kind: '(', start: i, argument: 0, fn: fn?.kind === 'function' ? fn : fn?.kind==='enum'?{params:[{type:fn.name,uri:fn.uri,line:fn.line}]}:undefined,
                appendType: appended?.rank ? { ...appended, rank: appended.rank - 1 } : undefined });
        } else if (['}', ']', ')'].includes(t.value)) {
            stack.pop();
        } else if (parent?.kind === '{') {
            if (t.value === ',') { parent.stage = 'key'; parent.key = undefined; }
            else if (parent.stage === 'key' && t.kind === 'id') parent.key = t;
            else if (t.value === '=' && parent.key) { parent.used.add(parent.key.value); parent.stage = 'value'; }
        } else if (parent?.kind === '(' && t.value === ',') parent.argument++;
    }
    if(valueOnly)return expected(tokens.length);
    const frame = stack.at(-1);
    if (frame?.kind !== '{' || !frame.model) return undefined;
    return { ...frame, candidates: named(frame).filter(p => frame.model.initializer || env.canAccess(p)),
        keySymbol: named(frame).find(p => p.name === frame.key?.value) };
}
module.exports = { initializationContext };
