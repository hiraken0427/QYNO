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

test('module exports exclude private functions constants models and interfaces', () => {
    const modules={lib:'private const LIMIT=3\nprivate helper(x)=x*2\nprivate model Hidden;\n\tint n\nprivate interface Internal;\n\tint get();\npublic int read()=helper(LIMIT)\nconst LABEL="ok"\npublic model Point;\n\tint x'};
    const result=labels('import lib\nint main();\n\tlib.¦',modules);
    assert.ok(result.includes('read'));assert.ok(result.includes('LABEL'));assert.ok(result.includes('Point'));
    for(const hidden of ['LIMIT','helper','Hidden','Internal'])assert.ok(!result.includes(hidden));
    assert.equal(at('import lib\nint main();\n\tlib.hel¦per(1)',modules),undefined);
    assert.equal(hint('import lib\nint main();\n\tlib.helper(¦',modules),undefined);
    assert.equal(hint('import lib\nint main();\n\tlib.read(¦',modules).label,'public int lib.read()');
});

test('module aliases and using-all preserve module privacy', () => {
    const modules={lib:'private const LIMIT=3\nprivate helper(x)=x\npublic read()=helper(LIMIT)'};
    const imported=labels('import lib\nusing lib\nint main();\n\t¦',modules);
    assert.ok(imported.includes('read'));assert.ok(!imported.includes('LIMIT'));assert.ok(!imported.includes('helper'));
    assert.ok(!labels('import lib\nusing lib as m\nint main();\n\tm.¦',modules).includes('helper'));
});

test('private definitions keep local navigation completion and explicit visibility', () => {
    const source='private const LIMIT=3\nprivate helper(x)=x+LIMIT\nprivate model Hidden;\n\tint n\npublic read()=helper(LIMIT)\nint main();\n\t¦';
    const result=items(source);
    assert.ok(result.some(i=>i.label==='LIMIT'&&i.detail==='private const auto LIMIT'));
    assert.ok(result.some(i=>i.label==='helper'&&i.detail==='private auto helper(auto x)'));
    assert.ok(result.some(i=>i.label==='Hidden'));
    assert.equal(at('private helper(x)=x\npublic read()=hel¦per(1)').line,0);
});

test('omitted and auto argument types are preserved in generic signatures', () => {
    const source = 'square(x) = x^2\nauto add(auto x, double y) = x + y\nint main();\n\t¦';
    const result = items(source);
    assert.equal(result.find(i => i.label === 'square').detail, 'auto square(auto x)');
    assert.equal(result.find(i => i.label === 'add').detail, 'auto add(auto x, double y)');
    assert.equal(hint('square(x) = x^2\nint main();\n\tsquare(¦').label, 'auto square(auto x)');
});

test('generic ref and readonly argument completion retains passing modes', () => {
    const source = 'first(ref readonly values) = values[0]\nint main();\n\tfi¦';
    const first = items(source).find(i => i.label === 'first');
    assert.equal(first.detail, 'auto first(ref readonly auto values)');
    assert.equal(first.insertText, 'first(ref ${1:values})');
    assert.equal(at('first(ref readonly values) = val¦ues[0]').name, 'values');
});

test('inferred parameters are completed inside their function and do not escape', () => {
    assert.ok(labels('square(x);\n\t¦').includes('x'));
    assert.equal(at('square(x);\n\treturn ¦x^2').line, 0);
    assert.ok(!labels('square(x) = x^2\nint main();\n\t¦').includes('x'));
});

test('generic model methods retain readonly and parameter details', () => {
    const source = 'model Counter;\n\tprivate int bias\n\treadonly add(x) = this.bias + x\nint main();\n\tCounter c\n\tc.¦';
    const add = items(source).find(i => i.label === 'add');
    assert.equal(add.detail, 'public readonly auto add(auto x)');
    assert.equal(add.insertText, 'add(${1:x})');
});

test('imported generic function arguments retain definitions and signature hints', () => {
    const modules = {helper:'square(x) = x^2\nfirst(ref readonly x) = x[0]'};
    assert.equal(hint('import helper\nint main();\n\thelper.square(¦', modules).label, 'auto helper.square(auto x)');
    assert.equal(at('import helper\nint main();\n\thelper.squ¦are(3)', modules).uri, 'file:///helper.qy');
    assert.equal(items('import helper\nusing helper\nint main();\n\tfi¦', modules).find(i => i.label === 'first').insertText, 'first(ref ${1:x})');
});

test('auto and omitted return functions support forward completion and signatures', () => {
    const source = 'int main();\n\t¦\nauto square(int x) = x^2\ntwice(int n) = n * 2';
    const result = items(source);
    assert.equal(result.find(i => i.label === 'square').detail, 'auto square(int x)');
    assert.equal(result.find(i => i.label === 'twice').detail, 'auto twice(int n)');
    assert.equal(hint('twice(int n) = n * 2\nint main();\n\ttwice(¦').label, 'auto twice(int n)');
    assert.equal(at('int main();\n\tprint(twi¦ce(3))\ntwice(int n) = n * 2').line, 2);
});

test('inferred return block functions retain typed ref parameters and local scope', () => {
    const source = 'sum(ref readonly int values[]);\n\ttotal = values[0]\n\treturn total\nint main();\n\t¦';
    assert.ok(!labels(source).includes('total'));
    assert.ok(!labels(source).includes('values'));
    const candidate = items(source).find(i => i.label === 'sum');
    assert.equal(candidate.detail, 'auto sum(ref readonly int values[])');
    assert.equal(candidate.insertText, 'sum(ref ${1:values})');
    assert.equal(at('sum(int value);\n\treturn val¦ue').line, 0);
});

test('ordinary call statements do not introduce inferred function declarations', () => {
    const model = analyze('int main();\n\tconsume(1)\n\tif(true);\n\t\tconsume(2)');
    assert.deepEqual(model.symbols.filter(s => s.kind === 'function').map(s => s.name), ['main']);
});

test('inferred method declarations retain visibility readonly and this type', () => {
    const source = 'model Point;\n\tprivate int n\n\tprivate readonly get() = this.n\n\tpublic readonly auto read() = this.get()\nint main();\n\tPoint p\n\tp.¦';
    const result = items(source);
    assert.ok(result.some(i => i.label === 'read' && i.detail === 'public readonly auto read()'));
    assert.ok(!result.some(i => i.label === 'get'));
    assert.ok(labels('model Point;\n\tprivate int n\n\treadonly get();\n\t\tthis.¦').includes('n'));
});

test('imported inferred functions support aliases definitions and argument hints', () => {
    const modules = {helper:'square(int n) = n^2\nauto twice(int n) = n * 2'};
    assert.deepEqual(labels('import helper\nusing helper as h\nint main();\n\th.¦', modules).sort(), ['square','twice']);
    assert.equal(at('import helper\nint main();\n\thelper.squ¦are(3)', modules).uri, 'file:///helper.qy');
    assert.equal(hint('import helper\nint main();\n\thelper.square(¦', modules).label, 'auto helper.square(int n)');
});

test('inferred expression functions do not leak parameters into later declarations', () => {
    const source = 'square(int secret) = secret^2\ntwice(int value) = value * 2\nint main();\n\t¦';
    assert.ok(!labels(source).includes('secret'));
    assert.ok(!labels(source).includes('value'));
});

test('global inferred constants are available before their declarations', () => {
    const result = items('int main();\n\t¦\nconst auto LIMIT = 3\nconst LABEL = "QYNO"');
    assert.equal(result.find(i => i.label === 'LIMIT').detail, 'const auto LIMIT');
    assert.equal(result.find(i => i.label === 'LABEL').detail, 'const auto LABEL');
    assert.equal(at('int main();\n\tprint(LA¦BEL)\nconst LABEL = "QYNO"').line, 2);
});

test('local shorthand constants obey their declaration position and block', () => {
    assert.ok(!labels('int main();\n\t¦\n\tconst LIMIT = 3').includes('LIMIT'));
    assert.equal(at('int main();\n\tconst LIMIT = 3\n\tprint(LI¦MIT)').line, 1);
    assert.ok(!labels('int main();\n\tif(true);\n\t\tconst INNER = 3\n\t¦').includes('INNER'));
    assert.ok(!labels('int main();\n\tconst SELF = SE¦').includes('SELF'));
});

test('imported inferred constants retain namespace completion and definition targets', () => {
    const modules = {settings:'const LIMIT = 3\nconst auto LABEL = "QYNO"'};
    assert.deepEqual(labels('import settings\nusing settings as cfg\nint main();\n\tcfg.¦', modules).sort(), ['LABEL','LIMIT']);
    assert.equal(at('import settings\nint main();\n\tprint(settings.LI¦MIT)', modules).uri, 'file:///settings.qy');
    const result = items('import settings\nusing settings\nint main();\n\tLA¦', modules);
    assert.equal(result.find(i => i.label === 'LABEL').detail, 'const auto LABEL');
});

test('shorthand constants remain readonly in member completion and outline data', () => {
    const model = analyze('const VALUES = [1, 2]\nint main();\n\treturn 0');
    const constant = model.symbols.find(s => s.name === 'VALUES');
    assert.equal(constant.kind, 'constant'); assert.equal(constant.readonly, true); assert.equal(constant.scopeId, 0);
    assert.ok(!labels('const VALUE = 3\nint main();\n\tVALUE.¦').includes('addr'));
});

test('implicit local declarations are completed and located with auto labels', () => {
    const result = items('int main();\n\tcount = 3\n\t¦');
    assert.equal(result.find(i => i.label === 'count').detail, 'auto count');
    assert.equal(at('int main();\n\tcount = 3\n\tprint(cou¦nt)').line, 1);
});

test('assignment to an existing outer binding does not create another declaration', () => {
    const text = 'int main();\n\tcount = 3\n\tif(true);\n\t\tcount = 4\n\t\tprint(count)\n\tprint(count)';
    assert.equal(analyze(text).symbols.filter(s => s.name === 'count').length, 1);
    assert.equal(at(text.replace('count = 4', 'cou¦nt = 4')).line, 1);
    assert.equal(at(text.replace('print(count)', 'print(cou¦nt)')).line, 1);
});

test('implicit names are not visible before or within their own initializer', () => {
    assert.ok(!labels('int main();\n\t¦\n\tcount = 3').includes('count'));
    assert.ok(!labels('int main();\n\tcount = cou¦').includes('count'));
    assert.ok(!labels('int main();\n\tif(true);\n\t\tinside = 3\n\t¦').includes('inside'));
});

test('explicit nested shadowing resolves initializer and later reads separately', () => {
    const prefix = 'int main();\n\tint count = 3\n\tif(true);\n\t\tauto count = count + 1\n\t\tprint(count)\n\tprint(count)';
    assert.equal(at(prefix.replace('= count +', '= cou¦nt +')).line, 1);
    assert.equal(at(prefix.replace('\t\tprint(count)', '\t\tprint(cou¦nt)')).line, 3);
    assert.equal(at(prefix.replace('\n\tprint(count)', '\n\tprint(cou¦nt)')).line, 1);
    assert.equal(items(prefix + '\n\tcou¦').filter(i => i.label === 'count').length, 1);
});

test('implicit declarations respect parameter and sibling block bindings', () => {
    const source = 'int change(int count);\n\tcount = 3\n\tif(true);\n\t\tvalue = 4\n\telse;\n\t\tvalue = 5\n\treturn count';
    const symbols = analyze(source).symbols;
    assert.equal(symbols.filter(s => s.name === 'count').length, 1);
    assert.equal(symbols.filter(s => s.name === 'value').length, 2);
    assert.ok(!labels(source + '\nint main();\n\t¦').includes('value'));
});

test('only simple local assignments can introduce inferred names', () => {
    for (const body of ['count += 1', 'count++', 'obj.field = 1', 'values[0] = 1'])
        assert.ok(!analyze('int main();\n\t' + body).symbols.some(s => s.implicit));
    assert.ok(!analyze('count = 1').symbols.some(s => s.implicit));
    assert.ok(!analyze('model Data;\n\tcount = 1').symbols.some(s => s.implicit));
    assert.ok(!analyze('int main();\n\tio = 1').symbols.some(s => s.implicit));
});

test('methods index inferred locals without introducing model fields', () => {
    const model = analyze('model Data;\n\tint get();\n\t\tresult = 3\n\t\treturn result');
    assert.ok(model.symbols.some(s => s.name === 'result' && s.implicit));
    assert.ok(!model.symbols.find(s => s.name === 'Data').members.some(s => s.name === 'result'));
});

test('auto locals and constants support names, navigation and declared type labels', () => {
    const source = 'int main();\n\tauto count = 3\n\tconst auto LIMIT = 10\n\t¦';
    const result = items(source);
    assert.equal(result.find(i => i.label === 'count').detail, 'auto count');
    assert.equal(result.find(i => i.label === 'LIMIT').detail, 'const auto LIMIT');
    assert.equal(at('int main();\n\tauto count = 3\n\tprint(cou¦nt)').line, 1);
    assert.equal(at('int main();\n\tconst auto LIMIT = 10\n\tprint(LI¦MIT)').line, 1);
    assert.ok(items('int main();\n\tau¦').some(i => i.label === 'auto' && i.kind === 'type'));
});

test('auto locals obey declaration position and block scope', () => {
    assert.ok(!labels('int main();\n\t¦\n\tauto later = 1').includes('later'));
    assert.ok(!labels('int main();\n\tauto self = se¦').includes('self'));
    assert.ok(labels('int main();\n\tif(true);\n\t\tauto inner = [1]\n\t\t¦').includes('inner'));
    assert.ok(!labels('int main();\n\tif(true);\n\t\tauto inner = [1]\n\t¦').includes('inner'));
});

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
test('stdlib catalog includes fixed-width conversions and close requirements', () => {
    assert.ok(STANDARD.some(s=>s.path==='lib.size'));
    assert.ok(STANDARD.some(s=>s.path==='io.path.normalize'));
    assert.ok(!STANDARD.some(s=>s.path==='bool'));
    assert.match(STANDARD.find(s=>s.path==='io.file.open').documentation,/close/);
    assert.equal(STANDARD.find(s=>s.path==='io.file.readline').params[1].ref,true);
});
test('incomplete source remains usable without parser errors', () => {
    for (const text of ['void fn(ref int ma¦','int main();\n\tio.¦','int main();\n\tprint("{ma¦','using ¦','import geometry.¦']) assert.doesNotThrow(()=>items(text));
});

const modelSource = 'model Point;\n\tdouble x = 0.0\n\tdouble y = 0.0\n\tvoid move(double dx, double dy);\n\t\tthis.x += dx\n\t\tthis.y += dy';
test('model types, instance fields and methods are completed', () => {
    const source = modelSource+'\nint main();\n\tPoint p = {x=3.0}\n\tp.¦';
    const result = items(source);
    for (const name of ['x','y','move']) assert.ok(result.some(i=>i.label===name),name);
    assert.equal(result.find(i=>i.label==='move').insertText,'move(${1:dx}, ${2:dy})');
    assert.ok(!labels(modelSource+'\nint main();\n\t¦').includes('move'));
    assert.ok(labels(modelSource+'\nint main();\n\t¦').includes('Point'));
});
test('this is scoped to model methods and fields require a receiver', () => {
    assert.ok(labels(modelSource+'\n\tvoid other();\n\t\tthis.¦').includes('x'));
    assert.ok(!labels(modelSource+'\nint main();\n\t¦').includes('this'));
    assert.ok(!labels(modelSource+'\n\tvoid other();\n\t\t¦').includes('x'));
});
test('nested model and array element member completion', () => {
    const prefix=modelSource+'\nmodel Box;\n\tPoint point\n\tPoint points[]\nint main();\n\tBox box\n\t';
    assert.ok(labels(prefix+'box.point.¦').includes('move'));
    assert.ok(labels(prefix+'box.points.¦').includes('add'));
    assert.ok(labels(prefix+'box.points[0].¦').includes('x'));
});
test('model methods provide definitions and signatures', () => {
    const prefix=modelSource+'\nint main();\n\tPoint p\n\t';
    assert.equal(at(prefix+'p.mo¦ve(1.0, 2.0)').line,3);
    assert.equal(hint(prefix+'p.move(1.0, ¦').activeParameter,1);
    assert.equal(hint(prefix+'p.move(1.0, ¦').label,'public void p.move(double dx, double dy)');
    assert.equal(at(prefix+'p.x¦').type,'double');
});
test('imported model types and model parameters are supported', () => {
    const modules={'geometry.circle':modelSource};
    assert.ok(labels('import geometry.circle\nusing geometry.circle as geo\nint main();\n\tgeo.Point p\n\tp.¦',modules).includes('move'));
    assert.ok(labels('import geometry.circle\nvoid change(ref geometry.circle.Point p);\n\tp.¦',modules).includes('x'));
});
test('nested imported model fields resolve types in their defining module', () => {
    const f=fixture('import geometry.circle\nusing geometry.circle as geo\nint main();\n\tgeo.Box box\n\tbox.point.¦');
    f.project.modules.set('geometry.circle', require('../src/analysis').analyze('import points\nusing points\nmodel Box;\n\tPoint point','file:///geometry.circle.qy'));
    f.project.modules.set('points', require('../src/analysis').analyze(modelSource,'file:///points.qy'));
    assert.ok(complete(f.project,f.line,f.character).some(i=>i.label==='move'));
});

const privateSource = 'model Counter;\n\tprivate int count = 7\n\tpublic int shown = 1\n\tprivate void hidden();\n\t\tthis.count++\n\tpublic int current() = this.count';
test('visibility declarations are parsed and public is the default', () => {
    const parsed = analyze(privateSource + '\n\tint implicit');
    const counter = parsed.symbols.find(s => s.kind === 'model');
    assert.deepEqual(counter.members.map(s => [s.name, s.visibility]), [['count','private'],['shown','public'],['hidden','private'],['current','public'],['implicit','public']]);
    assert.ok(counter.members.every(s => s.declaringModelId === counter.modelId));
    assert.ok(!labels(privateSource+'\nint main();\n\t¦').includes('hidden'));
});
test('outside completion filters private fields and methods', () => {
    const result = labels(privateSource+'\nint main();\n\tCounter c\n\tc.¦');
    assert.ok(result.includes('shown')); assert.ok(result.includes('current'));
    assert.ok(!result.includes('count')); assert.ok(!result.includes('hidden'));
    assert.ok(labels('int main();\n\tpri¦').includes('private'));
    assert.ok(labels('int main();\n\tpub¦').includes('public'));
});
test('same-model this and other instance completion includes private', () => {
    for (const receiver of ['this', 'other']) {
        const result = items(privateSource+`\n\tvoid test(Counter other);\n\t\t${receiver}.¦`);
        assert.ok(result.some(s => s.label === 'count' && s.detail === 'private int count'));
        assert.ok(result.some(s => s.label === 'hidden' && s.detail === 'private void hidden()'));
    }
    const mathBody = labels(privateSource+'\n\tint twice() = this.¦');
    assert.ok(mathBody.includes('count'));
    const nested = labels(privateSource+'\n\tvoid test();\n\t\tif(true);\n\t\t\tthis.¦');
    assert.ok(nested.includes('count'));
});
test('another model cannot complete private members of an owned instance', () => {
    const prefix = privateSource+'\nmodel Box;\n\tprivate Counter child\n\tvoid test();\n\t\tthis.child.';
    const result = labels(prefix+'¦');
    assert.ok(result.includes('current')); assert.ok(!result.includes('count')); assert.ok(!result.includes('hidden'));
});
test('private receiver chains cannot expose nested members in completion', () => {
    const prefix='model Child;\n\tint exposed\nmodel Box;\n\tprivate Child child\n\tpublic Child visible\nint main();\n\tBox b\n\t';
    assert.ok(!labels(prefix+'b.child.¦').includes('exposed'));
    assert.ok(labels(prefix+'b.visible.¦').includes('exposed'));
    assert.ok(!labels(privateSource+'\nint main();\n\tCounter cs[1]\n\tcs[0].¦').includes('count'));
    assert.ok(!labels(privateSource+'\nint main();\n\tCounter c\n\tref Counter alias = c.addr\n\talias.value.¦').includes('count'));
});
test('private definition and hover remain available for manually written uses', () => {
    const source=privateSource+'\nint main();\n\tCounter c\n\tc.co¦unt';
    const symbol=at(source);
    assert.equal(symbol.name,'count'); assert.equal(symbol.visibility,'private'); assert.equal(symbol.line,1);
    const detail=require('../src/language').completion(symbol);
    assert.equal(detail.detail,'private int count'); assert.match(detail.documentation,/private/);
    assert.equal(hint(privateSource+'\nint main();\n\tCounter c\n\tc.hidden(¦'),undefined);
    assert.equal(hint(privateSource+'\n\tvoid test(Counter c);\n\t\tc.hidden(¦').label,'private void c.hidden()');
});
test('import aliases and selected using do not elevate completion privileges', () => {
    const modules={ 'geometry.circle':privateSource };
    for (const [using,type] of [['using geometry.circle as lib','lib.Counter'], ['using geometry.circle.Counter','Counter'], ['using geometry.circle','Counter']]) {
        const result=labels(`import geometry.circle\n${using}\nint main();\n\t${type} c\n\tc.¦`,modules);
        assert.ok(result.includes('current')); assert.ok(!result.includes('count'));
    }
});
test('same spelling in two modules does not imply same nominal owner', () => {
    const result=labels('import geometry.circle\n'+privateSource+'\n\tvoid test(geometry.circle.Counter other);\n\t\tother.¦',{'geometry.circle':privateSource});
    assert.ok(result.includes('current')); assert.ok(!result.includes('count'));
});

const readonlySource='model Point;\n\tdouble x\n\tprivate readonly int secret() = 7\n\tpublic readonly double distance() = this.x\n\tpublic void move(double dx);\n\t\tthis.x += dx';
test('readonly methods and ref readonly parameters are parsed and labelled', () => {
    const model=analyze(readonlySource+'\ndouble measure(ref readonly Point p) = p.distance()');
    const method=model.symbols.find(s=>s.name==='distance');
    const fn=model.symbols.find(s=>s.name==='measure');
    assert.equal(method.readonly,true); assert.equal(fn.params[0].readonly,true); assert.equal(fn.params[0].ref,true);
    assert.equal(require('../src/analysis').functionLabel(method),'public readonly double distance()');
    assert.equal(require('../src/analysis').functionLabel(fn),'double measure(ref readonly Point p)');
    assert.equal(items(readonlySource+'\nint main();\n\tconst Point origin = {}\n\tori¦').find(s=>s.label==='origin').detail,'const Point origin');
});
test('const and readonly receivers complete fields and readonly methods only', () => {
    for (const prefix of ['int main();\n\tconst Point p = {}\n\t', 'void inspect(ref readonly Point p);\n\t']) {
        const result=items(readonlySource+'\n'+prefix+'p.¦');
        assert.ok(result.some(s=>s.label==='x' && s.detail==='public double x (readonly)'));
        assert.ok(result.some(s=>s.label==='distance')); assert.ok(!result.some(s=>['move','secret','addr'].includes(s.label)));
        assert.ok(result.some(s=>s.label==='value'));
    }
    assert.ok(labels(readonlySource+'\nint main();\n\tPoint p\n\tp.¦').includes('move'));
});
test('readonly this includes private readonly methods and leaves mutable other instances writable', () => {
    const prefix=readonlySource+'\n\tpublic readonly void inspect(ref Point other);\n\t\t';
    const result=labels(prefix+'this.¦');
    assert.ok(result.includes('secret')); assert.ok(result.includes('distance')); assert.ok(!result.includes('move'));
    assert.ok(labels(prefix+'other.¦').includes('move'));
});
test('readonly propagates through nested fields arrays and value without changing copies', () => {
    const prefix=readonlySource+'\nmodel Box;\n\tPoint child\n\tPoint children[]\n\tint numbers[]\nint main();\n\tconst Box box = {}\n\t';
    for (const receiver of ['box.child','box.children[0]','box.value.child']) {
        const result=labels(prefix+receiver+'.¦');
        assert.ok(result.includes('distance')); assert.ok(!result.includes('move'));
    }
    for (const receiver of ['box.children','box.numbers']) {
        const result=labels(prefix+receiver+'.¦');
        assert.ok(result.includes('length')); assert.ok(!result.some(s=>['add','remove_at','clear'].includes(s)));
    }
    assert.ok(labels(prefix+'Point copy = box.child\n\tcopy.¦').includes('move'));
});
test('ref readonly arrays of every scalar type filter mutating operations', () => {
    for (const type of ['int','float','double','bool','str','char','file']) {
        const result=labels(`void inspect(ref readonly ${type} a[][]);\n\ta[0].¦`);
        assert.ok(result.includes('length')); assert.ok(!result.includes('add')); assert.ok(!result.includes('clear'));
    }
});
test('readonly signatures definitions and manually entered mutating method hover remain available', () => {
    const prefix=readonlySource+'\nint main();\n\tconst Point p = {}\n\t';
    assert.equal(hint(prefix+'p.distance(¦').label,'public readonly double p.distance()');
    assert.equal(hint(prefix+'p.move(¦'),undefined);
    assert.equal(at(prefix+'p.mo¦ve(1.0)').line,4);
    const helper='double measure(ref readonly Point p) = p.distance()\n';
    assert.equal(items(readonlySource+'\n'+helper+'int main();\n\tmea¦').find(s=>s.label==='measure').insertText,'measure(ref ${1:p})');
});
test('qualified imported constant models preserve readonly across aliases and fields', () => {
    const modules={'geometry.circle':readonlySource+'\nconst Point ORIGIN = {}'};
    for (const receiver of ['geometry.circle.ORIGIN','geo.ORIGIN']) {
        const prefix='import geometry.circle\nusing geometry.circle as geo\nint main();\n\t';
        const result=labels(prefix+receiver+'.¦',modules);
        assert.ok(result.includes('distance')); assert.ok(!result.includes('move'));
        assert.equal(hint(prefix+receiver+'.distance(¦',modules).label,`public readonly double ${receiver}.distance()`);
    }
    assert.ok(labels('int main();\n\tread¦').includes('readonly'));
});

const initSource='model Rectangle;\n\tprivate double stored_width\n\tprivate double stored_height\n\tpublic bool init(double width, double height=1.0);\n\t\tthis.stored_width = width\n\t\tthis.stored_height = height\n\tpublic readonly double area() = this.stored_width * this.stored_height';
test('initializers and default argument recipes are analysed separately from methods', () => {
    const m=analyze(initSource+'\nmodel Container;\n\tvoid init(int values[]=[1,2], Rectangle r={width=3.0});\n\t\treturn');
    const rect=m.symbols.find(s=>s.kind==='model' && s.name==='Rectangle');
    assert.equal(rect.initializer.initializer,true);
    assert.equal(rect.initializer.params[1].defaultText,'1.0');
    const container=m.symbols.find(s=>s.kind==='model' && s.name==='Container');
    assert.equal(container.initializer.params[0].rank,1); assert.equal(container.initializer.params[1].rank,0);
    assert.equal(container.initializer.params[1].defaultText,'{width=3.0}');
    assert.ok(!labels(initSource+'\nint main();\n\tRectangle r = {width=1.0}\n\tr.¦').includes('init'));
});
test('initializer keys use public parameter names and show required/default contracts', () => {
    const result=items(initSource+'\nint main();\n\tRectangle r = {¦');
    assert.deepEqual(result.map(s=>s.label).sort(),['height','width']);
    assert.equal(result.find(s=>s.label==='width').insertText,'width=');
    assert.match(result.find(s=>s.label==='width').documentation,/必須/);
    assert.match(result.find(s=>s.label==='height').detail,/double height=1.0/);
    assert.match(result.find(s=>s.label==='height').documentation,/省略可能/);
    assert.ok(!result.some(s=>s.label==='stored_width'));
});
test('initializer completion removes supplied keys and does not duplicate equals', () => {
    assert.deepEqual(labels(initSource+'\nint main();\n\tRectangle r = {height=2.0, ¦'),['width']);
    assert.equal(items(initSource+'\nint main();\n\tRectangle r = {wi¦=3.0}')[0].insertText,'width');
    assert.ok(labels(initSource+'\nint main();\n\tRectangle r = {width=math.¦').includes('sqrt'));
});
test('multiline initializer return assignment and array elements infer the target model', () => {
    const bodies=['Rectangle r = {\n\t\t¦','Rectangle r = {width=1.0}\n\tr = {¦',
        'Rectangle rs[] = [{¦','Rectangle rows[][] = [[{¦', 'Rectangle rs[] = []\n\trs.add({¦'];
    for (const body of bodies) assert.ok(labels(initSource+'\nint main();\n\t'+body).includes('width'),body);
    assert.ok(labels(initSource+'\nRectangle create();\n\treturn {¦').includes('width'));
});
test('nested initializer keys resolve model-valued parameters in their defining module', () => {
    const source=initSource+'\nmodel Panel;\n\tprivate Rectangle frame\n\tvoid init(Rectangle bounds);\n\t\tthis.frame = bounds';
    assert.deepEqual(labels(source+'\nint main();\n\tPanel p = {¦'),['bounds']);
    assert.ok(labels(source+'\nint main();\n\tPanel p = {bounds={¦').includes('width'));
    assert.ok(labels(source+'\n\tvoid other();\n\t\tthis.frame = {¦').includes('width'));
});
test('ordinary call and method parameter contexts support model initializer keys', () => {
    const source=initSource+'\nvoid consume(Rectangle r, int n);\n\treturn\nmodel Box;\n\tvoid consume(Rectangle r);\n\t\treturn';
    assert.ok(labels(source+'\nint main();\n\tconsume({¦').includes('width'));
    assert.ok(labels(source+'\nint main();\n\tBox b\n\tb.consume({¦').includes('width'));
    assert.deepEqual(labels(source+'\nint main();\n\tunknown({wi¦'),[]);
});
test('constructor signatures and key definitions target the initializer parameters', () => {
    const prefix=initSource+'\nint main();\n\tRectangle r = {';
    assert.equal(hint(prefix+'height=¦').label,'public bool Rectangle.init(double width, double height=1.0)');
    assert.equal(hint(prefix+'height=¦').activeParameter,1);
    const target=at(prefix+'wi¦dth=3.0}');
    assert.equal(target.name,'width'); assert.equal(target.line,3); assert.equal(target.kind,'parameter');
    const inner=initSource+'\nint main();\n\tRectangle r = {width=math.sqrt(¦';
    assert.match(hint(inner).label,/sqrt/);
    const after=initSource+'\nvoid consume(Rectangle r, int second, int third);\n\treturn\nint main();\n\tconsume({width=3.0, height=4.0}, ¦';
    assert.equal(hint(after).activeParameter,1);
});
test('initializer keys honour imported model aliases and field-based models without init', () => {
    const modules={'geometry.circle':initSource};
    for (const type of ['geometry.circle.Rectangle','geo.Rectangle','Rectangle']) {
        const result=labels('import geometry.circle\nusing geometry.circle as geo\nusing geometry.circle.Rectangle\nint main();\n\t'+type+' r = {¦',modules);
        assert.ok(result.includes('width')); assert.ok(!result.includes('stored_width'));
    }
    const source='model Data;\n\tint n\n\tprivate int secret\n';
    assert.deepEqual(labels(source+'int main();\n\tData d = {¦'),['n']);
    assert.deepEqual(labels(source+'\tData create();\n\t\treturn {¦').sort(),['n','secret']);
});
test('top-level init remains an ordinary function and identifier', () => {
    assert.ok(labels('int init(int n) = n\nint main();\n\tin¦').includes('init'));
    assert.equal(hint('int init(int n) = n\nint main();\n\tinit(¦').label,'int init(int n)');
});

const interfaceSource = 'interface Shape;\n\treadonly double area();\n\tvoid move(double dx, double dy);';
test('interfaces expose contract methods without addr/value and keep readonly declarations', () => {
    assert.deepEqual(labels(interfaceSource+'\nvoid use(ref Shape value);\n\tvalue.¦').sort(),['area','move']);
    assert.deepEqual(labels(interfaceSource+'\nvoid use(ref readonly Shape value);\n\tvalue.¦'),['area']);
    const type=analyze(interfaceSource).symbols.find(s=>s.name==='Shape');
    assert.equal(type.kind,'interface'); assert.equal(type.members[0].readonly,true);
});
test('contract member definition hover and signature point to interface, not implementation', () => {
    const source=interfaceSource+'\nmodel Circle implements Shape;\n\treadonly double area() = 1.0\nvoid use(ref readonly Shape value);\n\t';
    assert.equal(at(source+'value.ar¦ea()').line,1);
    assert.equal(at(source+'value.ar¦ea()').declaringModelId,analyze(interfaceSource,'file:///main.qy').symbols[0].modelId);
    assert.equal(hint(source+'value.area(¦').label,'public readonly double value.area()');
    assert.equal(at(source+'val¦ue').ref,true); assert.equal(at(source+'val¦ue').readonly,true);
});
test('interface parameters and signature methods do not leak to module completions', () => {
    const source='interface Operations;\n\tint act(int secret);\n\tint other();\nint main();\n\t¦';
    const values=labels(source);
    assert.ok(values.includes('Operations')); assert.ok(!values.includes('act')); assert.ok(!values.includes('secret'));
});
test('implements name navigates to contract and multiple contracts remain ordinary type names', () => {
    const source=interfaceSource+'\nmodel Circle implements Sha¦pe;\n\tint n';
    assert.equal(at(source).kind,'interface'); assert.equal(at(source).line,0);
});
test('imported interface types work with aliases selected and whole using', () => {
    const modules={'geometry.circle':interfaceSource};
    for (const [using,type] of [['using geometry.circle as geo','geo.Shape'],['using geometry.circle.Shape','Shape'],['using geometry.circle','Shape']]) {
        assert.deepEqual(labels(`import geometry.circle\n${using}\nvoid show(ref readonly ${type} value);\n\tvalue.¦`,modules),['area']);
        assert.equal(at(`import geometry.circle\n${using}\nvoid show(ref readonly ${type} value);\n\tvalue.ar¦ea()`,modules).uri,'file:///geometry.circle.qy');
    }
});
test('interfaces cannot regain value operations or constructor keys in completion', () => {
    assert.deepEqual(labels(interfaceSource+'\nvoid use(ref Shape value);\n\tvalue.value.¦'),[]);
    assert.deepEqual(labels(interfaceSource+'\nint main();\n\tShape value={¦}'),[]);
});
test('plain readonly model methods retain their modifier without explicit public', () => {
    const prefix='model Counter;\n\treadonly int get() = 1\n\tvoid change();\n\t\treturn\nvoid inspect(ref readonly Counter value);\n\tvalue.¦';
    assert.deepEqual(labels(prefix),['value','get']);
});

test('four integer radix literals are single lexical numbers and shifts are single tokens',()=>{
    assert.deepEqual(lexLine('0b1111 0o17 0d15 0xF 0B1 0O7 0D8 0XF').map(t=>t.kind),Array(8).fill('number'));
    assert.deepEqual(lexLine('mask << 3 >> 1').map(t=>t.value),['mask','<<','3','>>','1']);
});
test('xor and bnot are keywords while band bor bxor remain identifiers',()=>{
    for (const word of ['xor','bnot']) assert.equal(items('int main();\n\t'+word+'¦').find(i=>i.label===word)?.kind,'keyword');
    const model=analyze('int band(int bor, int bxor) = bor + bxor');
    assert.equal(model.symbols.find(s=>s.name==='band').kind,'function');
});
test('format specification completion does not leak variable or function names',()=>{
    const prefix='int main();\n\tint value=255\n\t';
    assert.deepEqual(labels(prefix+'io.print("{value:¦}")'),['0b','0o','0d','0x']);
    assert.deepEqual(labels(prefix+'io.input("{value:0¦}")'),['0b','0o','0d','0x']);
    assert.deepEqual(labels(prefix+'io.print("{value:0x¦}")'),['0x']);
    assert.equal(at(prefix+'io.print("{value:0¦x}")'),undefined);
});
test('variable and array completion before radix suffix preserves definitions and positions',()=>{
    const prefix='int main();\n\tint numbers[][]\n\t';
    assert.ok(labels(prefix+'io.input("😀 {num¦bers[0][0]:0x}")').includes('numbers'));
    assert.equal(at(prefix+'io.print("{num¦bers[0][0]:0x}")').name,'numbers');
    assert.equal(at(prefix+'io.input("{num¦bers[0][0]:0o}")').line,1);
});
test('conversion signature help includes bases and resolves reordered keyword arguments',()=>{
    const str=hint('int main();\n\tstr text=str(255,prefix=¦');
    assert.match(str.label,/int base=10, bool prefix=false/); assert.equal(str.activeParameter,2);
    assert.equal(hint('int main();\n\tstr text=str(255,prefix=true,base=¦').activeParameter,1);
    assert.match(hint('int main();\n\tint n=int("ff",¦').label,/int base=0/);
    assert.equal(items('int main();\n\tstr¦').find(i=>i.label==='str').insertText,'str(${1:value})');
});
test('strings and colons inside interpolation expressions do not become format fields',()=>{
    const prefix='int main();\n\tint number\n\t';
    assert.deepEqual(labels(prefix+'io.print("{str(\'literal:¦\')}")'),[]);
    assert.match(hint(prefix+'io.print("{str(num¦ber,16,true):0x}")').label,/str/);
    assert.ok(labels(prefix+'io.print("{int(\'0xff\') + num¦ber:0x}")').includes('number'));
    const token=lexLine('io.print("{int("0xff"):0d}")').find(t=>t.kind==='string');
    assert.equal(token.interpolations.length,1); assert.equal(token.closed,true);
});
test('malformed or unfinished format strings remain usable while editing',()=>{
    for (const suffix of ['{value:','{value:0','{value:0x','{int("0xff") + value:0x']) {
        assert.doesNotThrow(()=>labels('int main();\n\tint value\n\tio.print("'+suffix+'¦'));
    }
});

test('integer widths and unsigned declarations keep full type labels and parameter definitions', () => {
    const source = 'unsigned int_64 count(ref unsigned int_8 value) = value\nint main();\n\tunsigned int_16 bytes[] = [1]\n\t¦';
    const result = items(source);
    assert.equal(result.find(i=>i.label==='count').detail,'unsigned int_64 count(ref unsigned int_8 value)');
    assert.equal(result.find(i=>i.label==='bytes').detail,'unsigned int_16 bytes[]');
    assert.equal(at('void f(unsigned int_8 value);\n\tprint(val¦ue)').start,22);
    assert.equal(hint('int main();\n\tunsigned int_64(¦').label,'unsigned int_64 unsigned int_64(integer / half / float / double / quadruple / str value, int base=0)');
});
test('unsigned for counters and integer keyword completions remain scoped', () => {
    const result = items('int main();\n\tfor(0 <= unsigned int_8 i < 3,i++);\n\t\t¦');
    assert.equal(result.find(i=>i.label==='i').detail,'unsigned int_8 i');
    assert.ok(result.some(i=>i.label==='int_32'));
    assert.ok(result.some(i=>i.label==='unsigned'));
});

test('default integer aliases are documented and wide count properties keep int64', () => {
    const source='int main();\n\tfor(0 <= i < 2,i++);\n\t\t¦';
    assert.equal(items(source).find(i=>i.label==='i').detail,'auto i');
    assert.ok(STANDARD.find(s=>s.path==='int').documentation.includes('32bit'));
    assert.equal(STANDARD.find(s=>s.path==='io.file.write').type,'int_64');
    assert.equal(items('int main();\n\tint values[]\n\tvalues.¦').find(i=>i.label==='length').detail,'int_64 length');
});

test('imaginary literals are numeric tokens and i alone remains an identifier',()=> {
    assert.deepEqual(lexLine('i i4.0 4.0i i4e-2 4e2i').map(t=>[t.kind,t.value]),[['id','i'],['number','i4.0'],['number','4.0i'],['number','i4e-2'],['number','4e2i']]);
});
test('complex declarations and array components provide precision-specific properties',()=> {
    assert.equal(items('int main();\n\tcomplex z\n\tz.¦').find(i=>i.label==='real').detail,'float real');
    assert.equal(items('int main();\n\tcomplex_128 values[]\n\tvalues[0].¦').find(i=>i.label==='imag').detail,'double imag');
    assert.ok(!labels('int main();\n\tcomplex z\n\tz.¦').includes('length'));
    assert.equal(at('int main();\n\tcomplex z\n\tprint(z.re¦al)').type,'float');
});
test('complex constructor names and named signatures match the compiler',()=> {
    const item=items('int main();\n\tcom¦').find(i=>i.label==='complex');
    assert.equal(item.insertText,'complex(real=${1:0.0}, imag=${2:0.0})');
    assert.equal(hint('int main();\n\tcomplex(imag=¦').activeParameter,1);
    assert.ok(hint('int main();\n\tcomplex_128(¦').label.startsWith('complex_128 complex_128('));
});

test('standard short names require using and qualified completions remain available',()=>{
    const bare='int main();\n\t¦';
    assert.ok(!labels(bare).includes('print'));
    assert.ok(!labels(bare).includes('input'));
    assert.ok(!labels(bare).includes('size'));
    assert.ok(labels(bare).includes('io.print'));
    assert.ok(labels(bare).includes('lib.size'));
    assert.ok(labels('using io, lib\nint main();\n\t¦').includes('print'));
    assert.ok(labels('using io, lib\nint main();\n\t¦').includes('size'));
    assert.equal(hint('int main();\n\tprint(¦'),undefined);
    assert.match(hint('using io\nint main();\n\tprint(¦').label,/print/);
});
test('lib and path namespaces support aliases selective using and nested completion',()=>{
    assert.ok(labels('int main();\n\tlib.¦').includes('size'));
    assert.ok(labels('int main();\n\tio.¦').includes('path'));
    assert.ok(labels('using io.path as p\nint main();\n\tp.¦').includes('with_extension'));
    assert.ok(labels('using io\nint main();\n\tpath.¦').includes('join'));
    assert.ok(!labels('using io\nint main();\n\t¦').includes('join'));
    assert.ok(labels('using io.path.join, lib.length\nint main();\n\t¦').includes('join'));
    assert.match(hint('using lib as info\nint main();\n\tinfo.length(¦').label,/int_64/);
    assert.equal(hint('io.path.join("dir", ¦').activeParameter,1);
});
test('block using does not leak library short names',()=>{
    assert.ok(labels('int main();\n\tif(true);\n\t\tusing lib\n\t\t¦').includes('size'));
    assert.ok(!labels('int main();\n\tif(true);\n\t\tusing lib\n\t\tlib.size(1)\n\t¦').includes('size'));
    assert.ok(labels('using ¦').includes('lib'));
    assert.ok(labels('using io.¦').includes('path'));
});
