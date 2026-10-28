'use strict';
const { test } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const textmate = require('vscode-textmate');
const oniguruma = require('vscode-oniguruma');
async function grammar(theme) {
    const wasm = fs.readFileSync(require.resolve('vscode-oniguruma/release/onig.wasm'));
    await oniguruma.loadWASM(wasm.buffer.slice(wasm.byteOffset, wasm.byteOffset + wasm.byteLength));
    const registry = new textmate.Registry({ theme, onigLib: Promise.resolve({ createOnigScanner: patterns => new oniguruma.OnigScanner(patterns), createOnigString: text => new oniguruma.OnigString(text) }), loadGrammar: async scope => scope === 'source.qyno' ? textmate.parseRawGrammar(fs.readFileSync(path.join(__dirname, '../syntaxes/qyno.tmLanguage.json'),'utf8'),'qyno.json') : null });
    return registry.loadGrammar('source.qyno');
}
test('TextMate highlights actual QYNO 0.27.1 syntax and resets one-line strings', async () => {
    const g = await grammar();
    function scope(text, word, expected) { const token = g.tokenizeLine(text).tokens.find(t => t.startIndex <= text.indexOf(word) && t.endIndex > text.indexOf(word)); assert.ok(token?.scopes.includes(expected), JSON.stringify(token)); }
    scope('complex z = 3.0+i4.0', 'complex', 'storage.type.qyno');
    scope('unsigned int_128 n', 'int_128', 'storage.type.qyno');
    scope('half h = 1.5', 'half', 'storage.type.qyno');
    scope('Void unit = ()', 'Void', 'storage.type.qyno');
    scope('private model Hidden;', 'private', 'storage.modifier.qyno');
    scope('private model Hidden;', 'Hidden', 'entity.name.type.qyno');
    scope('public interface Shape;', 'Shape', 'entity.name.type.qyno');
    scope('private const LIMIT=3', 'LIMIT', 'variable.other.constant.qyno');
    scope('void values[3]', 'void', 'storage.type.qyno');
    scope('quadruple q = 1e4000', 'quadruple', 'storage.type.qyno');
    scope('quadruple q = 1e4000', '1e4000', 'constant.numeric.qyno');
    scope('complex_128 z = 4.0i', 'complex_128', 'storage.type.qyno');
    scope('print(i4.0)', 'i4.0', 'constant.numeric.qyno');
    scope('print(4.0i)', '4.0i', 'constant.numeric.qyno');
    scope('print(i4e-2)', 'i4e-2', 'constant.numeric.qyno');
    scope('print(0b1111)','0b1111','constant.numeric.qyno');
    scope('print(0o17)','0o17','constant.numeric.qyno');
    scope('print(0d15)','0d15','constant.numeric.qyno');
    scope('print(0xFF)','0xFF','constant.numeric.qyno');
    scope('print(a xor b)','xor','keyword.operator.qyno');
    scope('print(bnot(value))','bnot','keyword.operator.qyno');
    scope('print(value << 3)','<<','keyword.operator.qyno');
    scope('print(value >> 3)','>>','keyword.operator.qyno');
    scope('print("{value:0x}")','0x','constant.other.format.qyno');
    scope('input("{numbers[0]:0b}")','0b','constant.other.format.qyno');
    scope('unsigned int_64 count', 'unsigned', 'storage.type.qyno');
    scope('unsigned int_64 count', 'int_64', 'storage.type.qyno');
    scope('unsigned int_64 count', 'count', 'variable.other.qyno');
    scope('unsigned int_64 total(unsigned int_8 v) = v', 'total', 'entity.name.function.qyno');
    scope('print(int_8(3))', 'int_8', 'storage.type.qyno');
    scope('private int count', 'private', 'storage.modifier.qyno');
    scope('auto count = 3', 'auto', 'storage.type.qyno');
    scope('auto count = 3', 'count', 'variable.other.qyno');
    scope('count = 3', 'count', 'variable.other.qyno');
    scope('const auto LIMIT = 10', 'auto', 'storage.type.qyno');
    scope('const auto LIMIT = 10', 'LIMIT', 'variable.other.constant.qyno');
    scope('const LIMIT = 10', 'LIMIT', 'variable.other.constant.qyno');
    scope('const LIMIT = 10', 'const', 'storage.modifier.qyno');
    scope('square(int x) = x^2', 'square', 'entity.name.function.qyno');
    scope('square(x) = x^2', 'square', 'entity.name.function.qyno');
    scope('square(auto x) = x^2', 'auto', 'storage.type.qyno');
    scope('increase(ref x);', 'ref', 'storage.modifier.qyno');
    scope('auto square(int x) = x^2', 'auto', 'storage.type.qyno');
    scope('private readonly get() = this.n', 'readonly', 'storage.modifier.qyno');
    scope('private readonly get() = this.n', 'get', 'entity.name.function.qyno');
    scope('if(true);', 'if', 'keyword.control.qyno');
    scope('while(true);', 'while', 'keyword.control.qyno');
    scope('public bool init(double width, double height=1.0);', 'init', 'entity.name.function.qyno');
    scope('public readonly double distance() = this.x', 'readonly', 'storage.modifier.qyno');
    scope('public readonly double distance() = this.x', 'distance', 'entity.name.function.qyno');
    scope('void inspect(ref readonly Point p);', 'readonly', 'storage.modifier.qyno');
    scope('const Point ORIGIN = {}', 'ORIGIN', 'variable.other.constant.qyno');
    scope('public int current() = this.count', 'public', 'storage.modifier.qyno');
    scope('model Point;', 'Point', 'entity.name.type.qyno');
    scope('Point point = {x=3.0}', 'Point', 'storage.type.qyno');
    scope('this.x += dx', 'this', 'variable.language.this.qyno');
    scope('void increase(ref int value);','increase','entity.name.function.qyno');
    scope('void increase(ref int value);','ref','storage.modifier.qyno');
    scope('\tint matrix[2][3]','matrix','variable.other.qyno');
    scope('for(10 > i >= 0, i--);','for','keyword.control.qyno');
    scope('interface Shape;','Shape','entity.name.type.qyno');
    scope('model Circle implements Shape;','implements','storage.modifier.qyno');
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
    assert.equal(pkg.displayName,'QYNO 0.27.1'); assert.equal(pkg.version,'0.27.1'); assert.equal(pkg.publisher,'hiraken0427');
    for (const file of [pkg.main,...pkg.contributes.grammars.map(g=>g.path),...pkg.contributes.snippets.map(s=>s.path),pkg.contributes.languages[0].configuration]) assert.ok(fs.existsSync(path.join(base,file)),file);
    const snippets=require('../snippets/qyno.json');
    assert.ok(snippets.Main.body.includes('\treturn 0'));
    for (const name of ['Read file','Write file']) assert.ok(snippets[name].body.some(l=>l.startsWith('io.file.close(')));
});

test('interpolation braces and expressions have code colors without string ancestors', async () => {
    const g = await grammar({ settings: [
        { settings: { foreground: '#FFFFFF', background: '#000000' } },
        { scope: 'string', settings: { foreground: '#FF8800' } },
        { scope: 'punctuation', settings: { foreground: '#0088FF' } },
        { scope: 'entity.name.function', settings: { foreground: '#FFFF00' } }
    ] });
    const scopes = (text, word) => g.tokenizeLine(text).tokens.find(t => t.startIndex <= text.indexOf(word) && t.endIndex > text.indexOf(word)).scopes;
    const color = (text, word) => {
        const tokens = g.tokenizeLine2(text).tokens, offset = text.indexOf(word);
        let index = 0;
        while (index + 2 < tokens.length && tokens[index + 2] <= offset) index += 2;
        return (tokens[index + 1] >>> 15) & 511;
    };
    for (const quote of ['"', "'"]) {
        const text = `io.print(${quote}hello {matrix[i][j] + f(2)}!${quote})`;
        for (const word of ['{', 'matrix', 'i]', '+', 'f(', '}']) {
            assert.ok(!scopes(text, word).some(scope => scope.startsWith('string.')), `${word}: ${scopes(text, word)}`);
        }
        assert.equal(color(text, 'matrix'), color('matrix', 'matrix'));
        assert.equal(color(text, '{'), color('{x=2}', '{'));
        assert.equal(color(text, '}'), color('{x=2}', '}'));
        assert.equal(color(text, 'f('), color('f(2)', 'f('));
        assert.notEqual(color(text, 'hello'), color(text, 'matrix'));
        assert.equal(color(text, 'hello'), color(text, '!'));
    }
    assert.ok(scopes('print("escaped \\{value\\}")', 'value').includes('string.quoted.double.qyno'));
    const nested = 'print("{str(\"nested {value}\")}")';
    assert.ok(!scopes(nested, 'value').some(scope => scope.startsWith('string.')));
    assert.ok(scopes(nested, 'nested').includes('string.quoted.double.qyno'));
});

test('enter indents semicolons outside strings/comments for every declaration form', () => {
    const config = require('../language-configuration.json');
    const patterns = [config.indentationRules.increaseIndentPattern, config.onEnterRules[0].beforeText].map(p => new RegExp(p));
    for (const regex of patterns) {
        for (const line of ['int main();', '\tfor(0 <= i < 3, i++);', '\tif(true); // comment', 'model Point;', 'interface Shape;',
            'square(x);', 'unsigned int_64 count();', 'Point make();', 'complex_128 value();', 'private readonly get();', '\telse;']) assert.ok(regex.test(line), line);
        for (const line of ['\treturn 0', '\tprint(";")', "\tprint(';')", '// int main();', '\tprint(1) // ;', '\tstr text = "semi; // hi"', '\tprint("escaped \\\" ;")']) assert.ok(!regex.test(line), line);
    }
    assert.equal(config.onEnterRules[0].action.indent, 'indent');
});

test('enum declarations and members keep enum scopes across lines',async()=>{
    const g=await grammar();
    const header=g.tokenizeLine('private enum sex;');
    assert.ok(header.tokens.some(t=>t.scopes.includes('entity.name.type.qyno')));
    const member=g.tokenizeLine('\tfemale = 3',header.ruleStack);
    assert.ok(member.tokens.some(t=>t.scopes.includes('variable.other.enummember.qyno')));
    const after=g.tokenizeLine('int main();',member.ruleStack);
    assert.ok(!after.tokens.some(t=>t.scopes.includes('meta.enum.qyno')));
    for(const line of ['match(gender);','\tcase male, female;'])
        assert.ok(g.tokenizeLine(line).tokens.some(t=>t.scopes.includes('keyword.control.qyno')));
});
