'use strict';
const { test } = require('node:test');
const assert = require('node:assert/strict');
const { analyze, lexLine } = require('../src/analysis');
const { complete, symbolAt, signature, environment } = require('../src/language');
const { STANDARD } = require('../src/catalog');
function fixture(text, modules = {}) {
    const offset = text.indexOf('¦'); assert.notEqual(offset, -1);
    const before = text.slice(0, offset), line = before.split('\n').length - 1, character = before.split('\n').at(-1).length;
    return { project: { model: analyze(text.replace('¦', ''), 'file:///main.qy'), modules: new Map(Object.entries(modules).map(([name, source]) => [name, analyze(source, `file:///${name}.qy`)])), moduleNames: ['geometry.circle', 'geometry.square'] }, line, character };
}
function items(text, modules) { const f = fixture(text, modules); return complete(f.project, f.line, f.character); }
function labels(text, modules) { return items(text, modules).map(i => i.label); }
function at(text, modules) { const f = fixture(text, modules); return symbolAt(f.project, f.line, f.character); }
function hint(text, modules) { const f = fixture(text, modules); return signature(f.project, f.line, f.character); }

test('forward functions, constants, parameters and locals are completed with types', () => {
    const result = items('int main(int arg);\n\tstr title\n\t¦\n\treturn later(arg)\nint later(int n) = n + 1\nconst int LIMIT = 10');
    for (const name of ['main', 'later', 'arg', 'title', 'LIMIT']) assert.ok(result.some(i => i.label === name), name);
    assert.match(result.find(i => i.label === 'later').detail, /int later\(int n\)/);
});
test('function locals and parameters do not leak into another function', () => {
    const result = labels('int first(int secret);\n\tint hidden\n\treturn 0\nint second();\n\t¦\n\treturn 0');
    assert.ok(!result.includes('secret')); assert.ok(!result.includes('hidden'));
});
test('locals are unavailable before declarations and in their own initializer', () => {
    assert.ok(!labels('int main();\n\t¦\n\tint later\n\treturn 0').includes('later'));
    assert.ok(!labels('int main();\n\tint self = se¦').includes('self'));
});
test('loop counters and nested variables respect indentation and sibling blocks', () => {
    assert.ok(labels('int main();\n\tfor(10 > i >= 0, i--);\n\t\t¦').includes('i'));
    assert.ok(!labels('int main();\n\tfor(0 <= i < 10, i++);\n\t\tint inner\n\t¦').includes('i'));
    assert.ok(!labels('int main();\n\tif(true);\n\t\tint inner\n\telse;\n\t\t¦').includes('inner'));
    const list = items('int main();\n\tfor(0.0 < double x <= 1.0, x += 0.1);\n\t\tx¦');
    assert.equal(list.find(i => i.label === 'x').detail, 'double x');
});
test('all primitive array types and suffix ranks are parsed', () => {
    for (const type of ['int','float','double','bool','str','char','file']) {
        const model = analyze(`void change(ref ${type} matrix[][]);\n\tmatrix[0][0] = matrix[0][0]`);
        assert.equal(model.symbols[0].rank, 2); assert.equal(model.symbols[0].ref, true);
    }
    assert.equal(analyze('int make()[][] = [[1]]').symbols.find(s => s.name === 'make').rank, 2);
});
test('namespace builtins are available without import, io aliases include child file', () => {
    assert.deepEqual(labels('int main();\n\tio.fi¦'), ['file']);
    assert.ok(labels('using io\nint main();\n\tfile.¦').includes('readline'));
    assert.match(hint('using io\nint main();\n\tfile.open(¦').label, /file.open/);
    assert.ok(labels('using io as console\nint main();\n\tconsole.file.¦').includes('close'));
    assert.ok(labels('using io.file as fs\nint main();\n\tfs.¦').includes('open'));
});
test('local using aliases do not leak; global aliases support forward use', () => {
    assert.ok(labels('int main();\n\tfs.¦\nusing io.file as fs').includes('open'));
    assert.ok(!labels('int a();\n\tusing io.file as fs\n\treturn 0\nint b();\n\t¦').includes('fs'));
    assert.ok(!labels('int main();\n\t¦\n\tusing math').includes('sqrt'));
});
const moduleSource = 'const double PI = 3.14\ndouble area(double radius) = radius * radius * PI\nvoid grow(ref int matrix[][]);\n\tmatrix.add()';
test('imported module completion and aliases expose only exported functions and constants', () => {
    const modules = {'geometry.circle': moduleSource};
    assert.deepEqual(labels('import geometry.circle\nusing geometry.circle as circle\nint main();\n\tcircle.¦', modules).sort(), ['PI','area','grow'].sort());
    assert.ok(labels('import geometry.circle\nusing geometry.circle\nint main();\n\tar¦', modules).includes('area'));
    assert.ok(!labels('int main();\n\tar¦', modules).includes('area'));
});
test('import paths and selected using completion', () => {
    assert.deepEqual(labels('import geometry.¦'), ['circle', 'square']);
    assert.ok(labels('import geometry.circle\nusing geometry.circle.ar¦', {'geometry.circle':moduleSource}).includes('area'));
    assert.deepEqual(labels('using io.file as fs¦'), []);
});
test('array and string member suggestions respect remaining dimensions', () => {
    const prefix = 'int main();\n\tint matrix[2][3]\n\t';
    assert.ok(labels(prefix + 'matrix.¦').includes('add'));
    assert.ok(labels(prefix + 'matrix[0].¦').includes('length'));
    assert.ok(!labels(prefix + 'matrix[0][0].¦').includes('length'));
    assert.ok(labels('int main();\n\tstr name\n\tname.¦').includes('length'));
    assert.ok(!labels('int main();\n\tstr name\n\tname.¦').includes('add'));
});
test('completion uses ref call arguments and avoids duplicate parentheses', () => {
    const source = 'void grow(ref int matrix[][]);\n\tmatrix.add()\nint main();\n\tgro¦';
    assert.equal(items(source).find(i=>i.label==='grow').insertText, 'grow(ref ${1:matrix})');
    assert.equal(items(source+'()').find(i=>i.label==='grow').insertText, 'grow');
});
test('no completion in comments or literal text; interpolation supports indexed destinations', () => {
    assert.deepEqual(labels('int main();\n\t// io.¦'), []);
    assert.deepEqual(labels('int main();\n\tprint("hello io.¦")'), []);
    assert.ok(labels('int main();\n\tint matrix[2][3]\n\tinput("入力> {mat¦}")').includes('matrix'));
    assert.deepEqual(labels('int main();\n\tprint("\\{mat¦}")'), []);
    assert.deepEqual(labels("int main();\n\tprint('text io.¦')"), []);
});
test('UTF-16 offsets, quotes, decimal numbers and comments match editor positions', () => {
    const source = 'io.print("😀 {name}") // hi';
    const tokens = lexLine(source);
    const string = tokens.find(t=>t.kind==='string');
    assert.equal(string.interpolations[0].tokens[0].start, source.indexOf('name'));
    assert.equal(tokens.at(-1).kind, 'comment');
    assert.deepEqual(lexLine('1.25 3').map(t=>t.kind), ['number','number']);
});
test('definitions locate local declarations and imported function targets', () => {
    assert.equal(at('int main();\n\tint va¦lue\n\treturn 0').name,'value');
    assert.equal(at('int main();\n\tint value\n\treturn val¦ue').line,1);
    assert.equal(at('import geometry.circle\nint main();\n\tgeometry.circle.ar¦ea(2.0)',{'geometry.circle':moduleSource}).uri,'file:///geometry.circle.qy');
});
test('signature help handles qualified aliases, ref arguments, nested calls and array commas', () => {
    assert.equal(hint('using io.file as fs\nint main();\n\tfs.readline(source, ¦').activeParameter, 1);
    assert.match(hint('using io.file as fs\nint main();\n\tfs.readline(source, ¦').label, /ref str line/);
    assert.equal(hint('int sum(int a, int b) = a + b\nint main();\n\tsum(math.abs(1), ¦').activeParameter,1);
    assert.equal(hint('void f(int a[], int b);\n\treturn\nint main();\n\tf([1, 2], ¦').activeParameter,1);
});
test('stdlib catalog matches 0.25.2 functions and close requirements', () => {
    assert.equal(STANDARD.length,19);
    assert.ok(!STANDARD.some(s=>s.path==='bool'));
    assert.match(STANDARD.find(s=>s.path==='io.file.open').documentation,/close/);
    assert.equal(STANDARD.find(s=>s.path==='io.file.readline').params[1].ref,true);
});
test('incomplete source remains usable without parser errors', () => {
    for (const text of ['void fn(ref int ma¦','int main();\n\tio.¦','int main();\n\tprint("{ma¦','using ¦','import geometry.¦']) assert.doesNotThrow(()=>items(text));
});
