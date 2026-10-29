'use strict';

const TYPES = new Set(['complex', 'complex_64', 'complex_128', 'int', 'int_8', 'int_16', 'int_32', 'int_64', 'int_128', 'half', 'quadruple', 'unsigned', 'float', 'double', 'bool', 'str', 'char', 'file', 'void', 'Void', 'auto']);
const KEYWORDS = new Set([...TYPES, 'const', 'ref', 'val', 'return', 'if', 'else', 'for', 'while', 'break', 'continue', 'and', 'or', 'xor', 'bnot', 'not', 'true', 'false', 'none', 'import', 'using', 'as', 'print', 'input', 'struct', 'enum', 'match', 'case', 'try', 'finally', 'throw', 'of', 'model', 'interface', 'implements', 'public', 'private', 'readonly']);
const identifierStart = c => /[A-Za-z_]/.test(c || '');
const identifierPart = c => /[A-Za-z0-9_]/.test(c || '');

function interpolationEnd(text, open) {
    let depth=1, quote=null;
    for (let i=open+1;i<text.length;i++) {
        if (text[i]==='\\') { i++; continue; }
        if (quote) { if (text[i]===quote) quote=null; continue; }
        if (text[i]==='"' || text[i]==="'") { quote=text[i]; continue; }
        if (text[i]==='{') depth++;
        if (text[i]==='}' && --depth===0) return i;
    }
    return -1;
}
function offsetToken(token, offset) {
    return {...token,start:token.start+offset,end:token.end+offset,
        ...(token.interpolations ? {interpolations:token.interpolations.map(p=>({...p,start:p.start+offset,end:p.end+offset,
            tokens:p.tokens.map(t=>offsetToken(t,offset)),formatStart:p.formatStart===undefined?undefined:p.formatStart+offset}))} : {})};
}
function formatStart(tokens) {
    let depth=0;
    for (const t of tokens) {
        if (t.kind==='string') continue;
        if (['(','[','{'].includes(t.value)) depth++;
        else if ([')',']','}'].includes(t.value)) depth--;
        else if (t.value===':' && depth===0) return t.end;
    }
    return undefined;
}

/** UTF-16 columns deliberately match VS Code positions, including emoji. */
function lexLine(text, line = 0) {
    const tokens = [];
    let i = 0;
    const add = (kind, start, end, extra = {}) => tokens.push({ kind, value: text.slice(start, end), start, end, line, ...extra });
    while (i < text.length) {
        const start = i;
        if (/\s|\uFEFF/.test(text[i])) { i++; continue; }
        if (text.startsWith('//', i)) { add('comment', i, text.length); break; }
        if (text[i] === '"' || text[i] === "'") {
            const quote = text[i++];
            const interpolations = [];
            let closed = false;
            while (i < text.length) {
                if (text[i] === '\\') { i = Math.min(text.length, i + 2); continue; }
                if (text[i] === quote) { i++; closed = true; break; }
                if (text[i] === '{') {
                    const open=i, begin=++i, found=interpolationEnd(text,open);
                    if (found>=0) i=found;
                    else while (i < text.length && text[i] !== '}' && text[i] !== quote) i++;
                    const end = i;
                    const nested = lexLine(text.slice(begin, end), line).map(t => offsetToken(t,begin));
                    interpolations.push({ start: begin, end, tokens: nested, formatStart:formatStart(nested) });
                    if (text[i] === '}') i++;
                    continue;
                }
                i++;
            }
            add('string', start, i, { closed, interpolations });
        } else if (/^(?:i[0-9]+(?:\.[0-9]+)?(?:[eE][+-]?[0-9]+)?|[0-9]+(?:\.[0-9]+)?(?:[eE][+-]?[0-9]+)?i)(?![A-Za-z0-9_])/.test(text.slice(i))) {
            const value=/^(?:i[0-9]+(?:\.[0-9]+)?(?:[eE][+-]?[0-9]+)?|[0-9]+(?:\.[0-9]+)?(?:[eE][+-]?[0-9]+)?i)(?![A-Za-z0-9_])/.exec(text.slice(i))[0];
            i+=value.length; add('number',start,i);
        } else if (identifierStart(text[i])) {
            while (identifierPart(text[++i])) { /* consume */ }
            add('id', start, i);
        } else if (/[0-9]/.test(text[i])) {
            if (text[i]==='0' && /[bodx]/i.test(text[i+1] || '')) {
                i+=2; while (/[A-Za-z0-9_]/.test(text[i] || '')) i++;
            } else {
                while (/[0-9]/.test(text[++i] || '')) { /* consume */ }
                if (text[i] === '.' && /[0-9]/.test(text[i + 1] || '')) {
                    i++; while (/[0-9]/.test(text[i] || '')) i++;
                }
                const exponent=/^[eE][+-]?[0-9]+/.exec(text.slice(i));
                if(exponent)i+=exponent[0].length;
            }
            add('number', start, i);
        } else {
            i += ['==', '!=', '<=', '>=', '++', '--', '+=', '-=', '<<', '>>'].includes(text.slice(i, i + 2)) ? 2 : 1;
            add('punct', start, i);
        }
    }
    return tokens;
}

function codeContext(model, line, character) {
    const text=model.lines[line] || '';
    const inside=token=>token.kind==='string' && character>token.start &&
        (character<token.end || !token.closed && character===token.end);
    const inString=token=> {
        const part=token.interpolations.find(p=>character>=p.start && character<=p.end);
        if (!part) return null;
        const nested=part.tokens.find(inside);
        if (nested) return inString(nested);
        const format=part.formatStart!==undefined && character>=part.formatStart;
        const start=format?part.formatStart:part.start;
        return {text:text.slice(start,character),start,tokens:part.tokens,format};
    };
    for (const token of model.tokens[line] || []) {
        if (token.kind==='comment' && character>=token.start) return null;
        if (inside(token)) return inString(token);
    }
    return {text:text.slice(0,character),start:0,tokens:model.tokens[line] || []};
}

function split(tokens, separator = ',') {
    const parts = []; let part = [], depth = 0;
    for (const token of tokens) {
        if (token.value === separator && depth === 0) { parts.push(part); part = []; continue; }
        if (['(', '[', '{'].includes(token.value)) depth++;
        if ([')', ']', '}'].includes(token.value)) depth--;
        part.push(token);
    }
    parts.push(part); return parts;
}
function closing(tokens, begin) {
    let depth = 0;
    for (let i = begin; i < tokens.length; i++) {
        if (tokens[i].value === '(') depth++;
        if (tokens[i].value === ')' && --depth === 0) return i;
    }
    return -1;
}
function rankOf(tokens) { return tokens.filter(t => t.value === '[').length; }
function visibilityLabel(symbol) { return symbol.declaringModelId ? `${symbol.visibility || 'public'} ` : symbol.moduleVisibility ? `${symbol.moduleVisibility} ` : ''; }
function typeLabel(symbol) { return `${visibilityLabel(symbol)}${symbol.kind === 'constant' ? 'const ' : ''}${symbol.ref ? `ref ${symbol.readonly ? 'readonly ' : ''}` : ''}${symbol.type} ${symbol.name}${'[]'.repeat(symbol.rank || 0)}${symbol.kind === 'field' && symbol.readonly ? ' (readonly)' : ''}${symbol.defaultText !== undefined ? `=${symbol.defaultText}` : ''}`; }
function functionLabel(symbol, name = symbol.name) {
    return `${visibilityLabel(symbol)}${symbol.readonly ? 'readonly ' : ''}${symbol.type} ${name}(${symbol.params.map(typeLabel).join(', ')})${'[]'.repeat(symbol.rank || 0)}`;
}

function readType(tokens, start) {
    let end = start;
    const unsigned = tokens[start]?.value === 'unsigned';
    if (unsigned) {
        if (!/^int(?:_(?:8|16|32|64|128))?$/.test(tokens[start + 1]?.value || '')) return null;
        end++;
    }
    if (tokens[end]?.kind !== 'id' || KEYWORDS.has(tokens[end].value) && !TYPES.has(tokens[end].value)) return null;
    while (tokens[end + 1]?.value === '.' && tokens[end + 2]?.kind === 'id') end += 2;
    if (tokens[end + 1]?.kind !== 'id') return null;
    return { name: tokens.slice(start, end + 1).map(t => t.value).join(unsigned ? ' ' : ''), end: end + 1 };
}

function analyze(text, uri = '') {
    const lines = text.split(/\r?\n/);
    const tokens = lines.map(lexLine);
    const scopes = [{ id: 0, parent: null, depth: 0, startLine: 0, endLine: lines.length - 1, kind: 'module' }];
    const symbols = [], imports = [], usings = [], stack = [scopes[0]], tryHeaders=new Map();
    const newScope = (line, depth, kind, parent, headerVisible = false) => {
        const scope = { id: scopes.length, parent: parent.id, depth, startLine: headerVisible ? line : line + 1, endLine: lines.length - 1, headerLine: line, kind };
        scopes.push(scope); return scope;
    };
    const declare = (token, properties) => {
        if (!token || token.kind !== 'id' || KEYWORDS.has(token.value)) return null;
        const symbol = { name: token.value, uri, line: token.line, start: token.start, end: token.end, visibleAfter: lines[token.line].length + 1, rank: 0, ref: false, ...properties };
        symbols.push(symbol); return symbol;
    };
    for (let line = 0; line < lines.length; line++) {
        const ts = tokens[line].filter(t => t.kind !== 'comment');
        if (!ts.length) continue;
        // Space indentation is tolerated while editing; the compiler still requires tabs.
        const whitespace = lines[line].match(/^[\t \uFEFF]*/)[0];
        const indent = (whitespace.match(/\t/g) || []).length + Math.floor((whitespace.match(/ /g) || []).length / 4);
        while (stack.length > 1 && stack.at(-1).depth > indent) stack.pop().endLine = line - 1;
        const scope = stack.at(-1);
        if (['import', 'using'].includes(ts[0].value)) {
            for (const part of split(ts.slice(1))) {
                const as = part.findIndex(t => t.value === 'as');
                const pathTokens = as < 0 ? part : part.slice(0, as);
                const path = pathTokens.map(t => t.value).join('');
                if (!/^[A-Za-z_]\w*(?:\.[A-Za-z_]\w*)*$/.test(path)) continue;
                const item = { path, alias: as >= 0 ? part[as + 1]?.value : null, scopeId: scope.id, line, start: pathTokens[0].start, end: pathTokens.at(-1).end, visibleAfter: lines[line].length };
                (ts[0].value === 'import' ? imports : usings).push(item);
            }
            continue;
        }
        const visibilityOffset=['public','private'].includes(ts[0]?.value)?1:0;
        if (['model', 'interface', 'enum'].includes(ts[visibilityOffset]?.value) && ts[visibilityOffset+1]?.kind === 'id') {
            const body = newScope(line, indent + 1, ts[visibilityOffset].value, scope, true);
            const model = declare(ts[visibilityOffset+1], { visibility:visibilityOffset?ts[0].value:'public', moduleVisibility:visibilityOffset?ts[0].value:undefined, kind: ts[visibilityOffset].value, type: ts[visibilityOffset+1].value, scopeId: scope.id, bodyScopeId: body.id, members: [], visibleAfter: 0 });
            if (model) model.modelId = `${uri}:${model.line}:${model.start}`;
            body.owner = model; stack.push(body); continue;
        }
        if(scope.kind==='enum' && ts[0]?.kind==='id' && !KEYWORDS.has(ts[0].value)){
            const previous=scope.owner.members.at(-1);
            const raw=ts[1]?.value==='='?ts.slice(2).map(t=>t.value).join(''):undefined;
            let enumValue;
            try{enumValue=raw===undefined?(previous?.enumValue===undefined && previous?undefined:(previous?.enumValue??-1)+1):Number(BigInt(raw.replace(/^0d/i,'')));}catch{}
            const member=declare(ts[0],{kind:'enumMember',type:scope.owner.name,readonly:true,visibility:scope.owner.visibility,
                declaringEnumId:scope.owner.modelId,enumValue,scopeId:scope.id,visibleAfter:0});
            if(member)scope.owner.members.push(member);
            continue;
        }
        let index = 0;
        const visibility = ['public', 'private'].includes(ts[index]?.value) ? ts[index++].value : 'public';
        const readonly = ts[index]?.value === 'readonly'; if (readonly) index++;
        const member = ['model', 'interface'].includes(scope.kind) ? { visibility, declaringModelId: scope.owner?.modelId } : scope.id===0 ? { visibility,moduleVisibility:visibilityOffset?visibility:undefined } : {};
        const constant = ts[index]?.value === 'const'; if (constant) index++;
        const modifier = ['ref', 'val'].includes(ts[index]?.value) ? ts[index++].value : null;
        if (constant && !['model', 'interface'].includes(scope.kind) && ts[index]?.kind === 'id' &&
            !KEYWORDS.has(ts[index].value) && ts[index + 1]?.value === '=') {
            declare(ts[index], { ...member, kind: 'constant', type: 'auto', readonly: true, scopeId: scope.id });
            continue;
        }
        const omittedReturn = (indent === 0 || ['model', 'interface'].includes(scope.kind)) &&
            ts[index]?.kind === 'id' && !KEYWORDS.has(ts[index].value) && ts[index + 1]?.value === '(';
        const declaredType = omittedReturn ? { name: 'auto', end: index } : readType(ts, index);
        if (declaredType) {
            const type = declaredType.name;
            index = declaredType.end - 1;
            const name = ts[index + 1];
            if ((indent === 0 || ['model', 'interface'].includes(scope.kind)) && ts[index + 2]?.value === '(') {
                const end = closing(ts, index + 2);
                const body = newScope(line, indent + 1, 'function', scope, true);
                const params = [];
                for (const part of split(ts.slice(index + 3, end < 0 ? ts.length : end))) {
                    let p = 0; const ref = part[p]?.value === 'ref';
                    if (['ref', 'val'].includes(part[p]?.value)) p++;
                    const readonly = ref && part[p]?.value === 'readonly'; if (readonly) p++;
                    const omittedParameter = part[p]?.kind === 'id' && !KEYWORDS.has(part[p].value) &&
                        (!part[p + 1] || part[p + 1].value === '[');
                    const parameterType = omittedParameter ? { name: 'auto', end: p } : readType(part, p);
                    if (!parameterType) continue;
                    p = parameterType.end - 1;
                    const equal = part.findIndex((t, n) => n > p + 1 && t.value === '=');
                    const parameter = declare(part[p + 1], { kind: 'parameter', type: parameterType.name, rank: rankOf(part.slice(p + 2, equal < 0 ? part.length : equal)), ref, readonly, scopeId: body.id, visibleAfter: part[p + 1]?.end || 0,
                        ...(equal < 0 ? {} : { defaultText: lines[line].slice(part[equal].end, part.at(-1).end).trim() }) });
                    if (parameter) params.push(parameter);
                }
                const fn = declare(name, { ...member, readonly, initializer: scope.kind === 'model' && name.value === 'init', kind: 'function', type, rank: end < 0 ? 0 : rankOf(ts.slice(end + 1).filter(t => t.value !== '=')), params, scopeId: scope.id, bodyScopeId: body.id, visibleAfter: 0 });
                body.owner = fn;
                if (['model', 'interface'].includes(scope.kind)) {
                    scope.owner?.members.push(fn);
                    if (fn?.initializer && scope.owner) scope.owner.initializer = fn;
                    symbols.push({ name: 'this', type: scope.owner?.name, rank: 0, ref: true, readonly, kind: 'parameter', scopeId: body.id, line, start: -1, end: -1, visibleAfter: 0 });
                }
                // Return dimensions end at '=' or ';', not at an array in the body.
                if (fn && end >= 0) { const suffix = ts.slice(end + 1); const stop = suffix.findIndex(t => ['=', ';'].includes(t.value)); fn.rank = rankOf(stop < 0 ? suffix : suffix.slice(0, stop)); }
                if (end >= 0 && ts[end + 1]?.value === '=') body.endLine = line;
                else if (scope.kind !== 'interface' && ts.some(t => t.value === ';')) stack.push(body);
                else body.endLine = line;
                continue;
            }
            const end = ts.findIndex((t, n) => n > index + 1 && t.value === '=');
            const variable = declare(name, { ...member, kind: scope.kind === 'model' ? 'field' : constant ? 'constant' : 'variable', type, rank: rankOf(ts.slice(index + 2, end < 0 ? ts.length : end)), ref: modifier === 'ref', readonly: constant, scopeId: scope.id });
            if (scope.kind === 'model' && variable) scope.owner?.members.push(variable);
        }
        if (!declaredType && ts[0]?.kind === 'id' && !KEYWORDS.has(ts[0].value) && ts[1]?.value === '=' &&
            stack.some(s => s.kind === 'function')) {
            const name = ts[0].value, ancestors = new Set(stack.map(s => s.id));
            const bound = symbols.some(s => s.name === name && ancestors.has(s.scopeId) &&
                ['variable', 'constant', 'parameter', 'function', 'model', 'interface', 'enum'].includes(s.kind));
            const namespace = ['io', 'math', 'lib', 'time', ...imports.map(i => i.path.split('.')[0]),
                ...usings.filter(u => ancestors.has(u.scopeId)).map(u => u.alias)].includes(name);
            if (!bound && !namespace) declare(ts[0], { kind: 'variable', type: 'auto', implicit: true, scopeId: scope.id });
        }
        if (ts[0].value === 'for') {
            let depth = 0, counter = -1;
            for (let i = 1; i < ts.length; i++) {
                if (ts[i].value === '(') depth++;
                if (ts[i].value === ')') depth--;
                if (depth === 1 && ['<', '<=', '>', '>='].includes(ts[i].value)) { counter = i + 1; break; }
            }
            const body = newScope(line, indent + 1, 'for', scope, true);
            if (counter >= 0) {
                const typed = TYPES.has(ts[counter]?.value) ? readType(ts, counter) : null;
                const token = ts[typed ? typed.end : counter];
                declare(token, { kind: 'variable', type: typed ? typed.name : 'auto', loop: true, scopeId: body.id, visibleAfter: token?.end || 0 });
            }
            stack.push(body);
        } else if(ts[0].value==='match'){
            const body=newScope(line,indent+1,'match',scope);body.targetText=ts.slice(2,ts.findLastIndex(t=>t.value===')')).map(t=>t.value).join('');stack.push(body);
        } else if(ts[0].value==='try'){
            const as=ts.findIndex(t=>t.value==='as');
            tryHeaders.set(`${scope.id}:${indent}`,as<0?null:ts[as+1]);
            if(ts.some(t=>t.value===';'))stack.push(newScope(line,indent+1,'try',scope));
        } else if(ts[0].value==='case'&&ts[1]?.value==='('&&ts.some(t=>t.value===';')){
            const body=newScope(line,indent+1,'case',scope),binding=tryHeaders.get(`${scope.id}:${indent}`);
            if(binding)declare(binding,{kind:'variable',type:'io.Error',readonly:true,scopeId:body.id,visibleAfter:0});
            stack.push(body);
        } else if(ts[0].value==='finally'&&ts.some(t=>t.value===';')){
            tryHeaders.delete(`${scope.id}:${indent}`);stack.push(newScope(line,indent+1,'finally',scope));
        } else if (['if', 'else', 'while', 'case'].includes(ts[0].value) && ts.some(t => t.value === ';')) {
            stack.push(newScope(line, indent + 1, ts[0].value, scope));
        }
    }
    return { uri, text, lines, tokens, scopes, symbols, imports, usings };
}

function scopeContains(model, scopeId, line, character) {
    const scope = model.scopes[scopeId];
    if (!scope || line < scope.startLine || line > scope.endLine) return false;
    if (scope.depth && !model.lines[line]?.trim() && character < scope.depth) return false;
    return scope.parent === null || scopeContains(model, scope.parent, line, character);
}
function visibleSymbols(model, line, character) {
    return model.symbols.filter(s => s.kind !== 'field' && !['model', 'interface', 'enum'].includes(model.scopes[s.scopeId]?.kind) && scopeContains(model, s.scopeId, line, character) &&
        (s.scopeId === 0 || s.line < line || s.line === line && s.visibleAfter <= character));
}
function visibleUsings(model, line, character) {
    return model.usings.filter(u => scopeContains(model, u.scopeId, line, character) &&
        (u.scopeId === 0 || u.line < line || u.line === line && u.visibleAfter <= character));
}
module.exports = { TYPES, KEYWORDS, lexLine, codeContext, analyze, split, closing, visibleSymbols, visibleUsings, scopeContains, typeLabel, functionLabel };
