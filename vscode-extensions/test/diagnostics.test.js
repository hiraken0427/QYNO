'use strict';
const { test } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs/promises');
const syncFs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { pathToFileURL } = require('node:url');
const { analyze } = require('../src/analysis');
const { utf16Column, parseDiagnostics, checkProject, registerDiagnostics } = require('../src/diagnostics');
const compiler = process.env.QYNO_TEST_COMPILER || path.resolve(__dirname, '../../qyc');
const native = { skip: !syncFs.existsSync(compiler) && 'Build qyc or set QYNO_TEST_COMPILER to run compiler integration tests.' };
const source = (text, filename = path.resolve('/project/main.qy')) => analyze(text, pathToFileURL(filename).toString());

test('UTF-8 byte positions map tabs, Japanese, emoji and BOM to editor UTF-16 columns', () => {
    assert.equal(utf16Column('\t日本😀name', Buffer.byteLength('\t日本😀')), 5);
    assert.equal(utf16Column('日本', 1000), 2);
    const text = '\uFEFFint main();\r\n\tprint("日本😀", missing)\r\n';
    const filename = path.resolve('diagnostic.qy');
    const files = new Map([[process.platform === 'win32' ? filename.toLowerCase() : filename, { uri: 'file:///real/main.qy', text }]]);
    const bytes = Buffer.byteLength('\tprint("日本😀", ') + 1;
    const result = parseDiagnostics(`${filename}:2:${bytes}: エラー: 未宣言\r\n${filename}:1:5: 警告: test\r\nimport 経路: test`, files);
    assert.equal(result.length, 2); assert.equal(result[0].start, 15); assert.equal(result[0].end, 22);
    assert.equal(result[0].severity, 'error'); assert.equal(result[1].severity, 'warning');
    assert.equal(result[1].start, 5); assert.equal(result[0].uri, 'file:///real/main.qy');
    assert.deepEqual(parseDiagnostics('/unknown/file.qy:2:1: エラー: no', files), []);
});

test('snapshot failure and cancellation clean up temporary files', async () => {
    const before = (await fs.readdir(os.tmpdir())).filter(name => name.startsWith('qyno-check-')).sort();
    const project = { model: source('int main();\n\treturn 0\n'), modules: new Map() };
    const missing = await checkProject(path.join(os.tmpdir(), 'qyno-missing-compiler', 'qyc'), project);
    assert.match(missing.issue, /qyno.compilerPath/);
    const abort = new AbortController(); abort.abort();
    assert.equal((await checkProject(compiler, project, abort.signal)).cancelled, true);
    const after = (await fs.readdir(os.tmpdir())).filter(name => name.startsWith('qyno-check-')).sort();
    assert.deepEqual(after, before);
});

for (const [name, text, message] of [
    ['syntax error', 'int main();\n\tint x = (3 + )\n\treturn 0\n', /式|必要|構文/],
    ['type mismatch', 'int main();\n\tint x = "hello"\n\treturn 0\n', /int|str|型/],
    ['undeclared variable', 'int main();\n\tio.print(missing)\n\treturn 0\n', /missing/],
    ['undeclared function', 'int main();\n\tmissing_call()\n\treturn 0\n', /missing_call/],
    ['interpolation destination', 'int main();\n\tio.input("入力> {unknown}")\n\treturn 0\n', /unknown/],
    ['invalid complex operation', 'int main();\n\tcomplex a = i4\n\tbool x = a < i5\n\treturn 0\n', /複素|complex|比較|演算/]
]) test(`compiler diagnostics: ${name}`, native, async () => {
    const model = source(text);
    const result = await checkProject(compiler, { model, modules: new Map(), roots: [path.resolve('/project'), path.resolve('/unused-module-root')] });
    assert.equal(result.issue, undefined); assert.ok(result.diagnostics.some(d => d.severity === 'error' && message.test(d.message)), JSON.stringify(result));
    assert.equal(result.diagnostics.at(-1).uri, model.uri);
});

test('valid inference and complex input do not generate false errors or project outputs', native, async () => {
    const directory = await fs.mkdtemp(path.join(os.tmpdir(), 'qyno-editor-test-'));
    try {
        const entry = path.join(directory, 'main.qy');
        const original = 'int main();\n\treturn 0\n';
        await fs.writeFile(entry, original);
        const model = source('using io\nint main();\n\tn = 3\n\tcomplex z = 3 + i4\n\tio.print("{n}: {z}")\n\treturn 0\n', entry);
        const result = await checkProject(compiler, { model, modules: new Map() });
        assert.equal(result.issue, undefined); assert.ok(!result.diagnostics.some(d => d.severity === 'error'));
        assert.equal(await fs.readFile(entry, 'utf8'), original);
        assert.deepEqual(await fs.readdir(directory), ['main.qy']);
    } finally { await fs.rm(directory, { recursive: true, force: true }); }
});

test('unsaved imported files and module roots keep diagnostics on the original URI', native, async () => {
    const root = path.resolve('/project'), modules = path.resolve('/shared/modules');
    const entry = source('import geometry.circle\nint main();\n\tio.print(geometry.circle.area())\n\treturn 0\n', path.join(root, 'main.qy'));
    const imported = source('int area() = "bad"\n', path.join(modules, 'geometry/circle.qy'));
    const project = { model: entry, modules: new Map([['geometry.circle', imported]]), roots: [root, modules] };
    const result = await checkProject(compiler, project);
    assert.equal(result.issue, undefined); assert.equal(result.diagnostics.at(-1).uri, imported.uri);
    imported.lines = ['int area() = 3', ''];
    assert.deepEqual((await checkProject(compiler, project)).diagnostics, []);
});

test('entry imports retain identity and detect a circular import', native, async () => {
    const model = source('import main\nint main();\n\treturn 0\n');
    const result = await checkProject(compiler, { model, modules: new Map([['main', model]]) });
    assert.ok(result.diagnostics.some(d => /循環/.test(d.message)), JSON.stringify(result));
});

test('new untitled documents receive diagnostics on their untitled URI', native, async () => {
    const model = analyze('int main();\n\tio.print(missing)\n\treturn 0\n', 'untitled:Untitled-1');
    const result = await checkProject(compiler, { model, modules: new Map(), roots: [path.resolve('/project'), path.resolve('/unused-module-root')] });
    assert.equal(result.issue, undefined);
    assert.ok(result.diagnostics.some(d => d.uri === model.uri && /missing/.test(d.message)));
});

test('compiler warnings retain warning severity without becoming errors', native, async () => {
    const model = source('int main();\n\tunused = 3\n\treturn 0\n');
    const result = await checkProject(compiler, { model, modules: new Map() });
    assert.equal(result.issue, undefined);
    assert.ok(result.diagnostics.some(d => d.severity === 'warning' && /未使用/.test(d.message)));
    assert.ok(!result.diagnostics.some(d => d.severity === 'error'));
});

test('disabled diagnostics and untrusted workspaces do not load sources or launch the compiler', async () => {
    for (const [trusted, enabled] of [[false, true], [true, false]]) {
        let command;
        const disposable = { dispose() {} }, event = () => disposable;
        const document = { languageId: 'qyno', uri: { scheme: 'file' } };
        const collection = { ...disposable, clear() {}, delete() {}, set() { assert.fail('must not publish diagnostics'); } };
        const vscode = {
            languages: { createDiagnosticCollection: () => collection },
            workspace: { isTrusted: trusted, getConfiguration: () => ({ get: (name, fallback) => name === 'diagnostics.enabled' ? enabled : fallback }),
                onDidOpenTextDocument: event, onDidChangeTextDocument: event, onDidSaveTextDocument: event,
                onDidCloseTextDocument: event, onDidChangeConfiguration: event, onDidGrantWorkspaceTrust: event },
            window: { activeTextEditor: { document }, onDidChangeActiveTextEditor: event },
            commands: { registerCommand(name, callback) { command = callback; return disposable; } }
        };
        const context = { subscriptions: [] };
        registerDiagnostics(vscode, context, { base() { assert.fail('must not load sources'); } });
        await command();
        for (const item of context.subscriptions) item.dispose();
    }
});
