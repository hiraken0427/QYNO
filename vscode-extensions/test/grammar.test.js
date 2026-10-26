'use strict';
const { test } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const textmate = require('vscode-textmate');
const oniguruma = require('vscode-oniguruma');
async function grammar() {
    const wasm = fs.readFileSync(require.resolve('vscode-oniguruma/release/onig.wasm'));
    await oniguruma.loadWASM(wasm.buffer.slice(wasm.byteOffset, wasm.byteOffset + wasm.byteLength));
    const registry = new textmate.Registry({ onigLib: Promise.resolve({ createOnigScanner: patterns => new oniguruma.OnigScanner(patterns), createOnigString: text => new oniguruma.OnigString(text) }), loadGrammar: async scope => scope === 'source.qyno' ? textmate.parseRawGrammar(fs.readFileSync(path.join(__dirname, '../syntaxes/qyno.tmLanguage.json'),'utf8'),'qyno.json') : null });
    return registry.loadGrammar('source.qyno');
}
test('TextMate highlights actual QYNO 0.25.2 syntax and resets one-line strings', async () => {
    const g = await grammar();
    function scope(text, word, expected) { const token = g.tokenizeLine(text).tokens.find(t => t.startIndex <= text.indexOf(word) && t.endIndex > text.indexOf(word)); assert.ok(token?.scopes.includes(expected), JSON.stringify(token)); }
    scope('void increase(ref int value);','increase','entity.name.function.qyno');
    scope('void increase(ref int value);','ref','storage.modifier.qyno');
    scope('\tint matrix[2][3]','matrix','variable.other.qyno');
    scope('for(10 > i >= 0, i--);','for','keyword.control.qyno');
    scope('using io.file as fs','using','keyword.control.import.qyno');
    scope('io.file.close(source)','close','support.function.qyno');
    scope('const int SIZE = 10','SIZE','variable.other.constant.qyno');
    scope('io.print("入力> {matrix[i][j]}")','matrix','variable.other.qyno');
    scope('io.print("text // not comment")','text','string.quoted.double.qyno');
    scope('io.print(1) // comment','//','comment.line.double-slash.qyno');
    const first = g.tokenizeLine('"unfinished');
    const next = g.tokenizeLine('int number',first.ruleStack);
    assert.ok(!next.tokens.some(t=>t.scopes.some(s=>s.startsWith('string.'))));
});
test('manifest paths and snippets exist with QYNO indentation', () => {
    const base=path.join(__dirname,'..'), pkg=require('../package.json');
    assert.equal(pkg.version,'0.25.2'); assert.equal(pkg.publisher,'hiraken0427');
    for (const file of [pkg.main,...pkg.contributes.grammars.map(g=>g.path),...pkg.contributes.snippets.map(s=>s.path),pkg.contributes.languages[0].configuration]) assert.ok(fs.existsSync(path.join(base,file)),file);
    const snippets=require('../snippets/qyno.json');
    assert.ok(snippets.Main.body.includes('\treturn 0'));
    for (const name of ['Read file','Write file']) assert.ok(snippets[name].body.some(l=>l.startsWith('io.file.close(')));
});
