'use strict';

const fs = require('node:fs/promises');
const os = require('node:os');
const path = require('node:path');
const { fileURLToPath } = require('node:url');
const { execFile } = require('node:child_process');

const key = filename => process.platform === 'win32' ? path.resolve(filename).toLowerCase() : path.resolve(filename);

/** Compiler columns count UTF-8 bytes; VS Code columns count UTF-16 units. */
function utf16Column(text, bytes) {
    let column = 0, consumed = 0;
    for (const character of text) {
        const size = Buffer.byteLength(character);
        if (consumed + size > bytes) break;
        consumed += size; column += character.length;
    }
    return column;
}

function parseDiagnostics(stderr, files) {
    const result = [];
    for (const line of stderr.split(/\r?\n/)) {
        const match = /^(.+):(\d+):(\d+): (エラー|警告|error|warning): (.*)$/.exec(line);
        if (!match) continue;
        const source = files.get(key(match[1]));
        if (!source) continue;
        const lines = source.text.split(/\r?\n/);
        const row = Math.min(lines.length - 1, Math.max(0, Number(match[2]) - 1));
        const bom = row === 0 && lines[0].startsWith('\uFEFF') ? 1 : 0;
        const start = bom + utf16Column(lines[row].slice(bom), Math.max(0, Number(match[3]) - 1));
        const word = /^[A-Za-z_][A-Za-z0-9_]*|^(?:==|!=|<=|>=|<<|>>|\+\+|--)|^./u.exec(lines[row].slice(start));
        result.push({ uri: source.uri, line: row, start, end: start + (word?.[0].length || 0),
            severity: /^(警告|warning)$/.test(match[4]) ? 'warning' : 'error', message: match[5] });
    }
    return result;
}

/** Snapshot only the import graph, never rewrite source files or create project outputs. */
async function checkProject(command, project, signal) {
    const directory = await fs.mkdtemp(path.join(os.tmpdir(), 'qyno-check-'));
    const files = new Map();
    try {
        // Retain source directory relationships and import-root precedence. An import
        // back to the entry therefore keeps its identity (and remains a cycle).
        const snapshotPath = filename => {
            const absolute = path.resolve(filename), root = path.parse(absolute).root;
            return path.join(directory, 'sources', Buffer.from(root).toString('hex'), absolute.slice(root.length));
        };
        const isFile = project.model.uri.startsWith('file:');
        const entry = isFile ? snapshotPath(fileURLToPath(project.model.uri)) : path.join(directory, 'untitled', 'main.qy');
        const write = async (filename, model) => {
            const source = { uri: model.uri, text: model.lines.join('\n') };
            await fs.mkdir(path.dirname(filename), { recursive: true });
            await fs.writeFile(filename, source.text, 'utf8');
            files.set(key(filename), source);
        };
        await write(entry, project.model);
        for (const [name, model] of project.modules) {
            if (!/^[A-Za-z_]\w*(?:\.[A-Za-z_]\w*)*$/.test(name)) continue;
            await write(snapshotPath(fileURLToPath(model.uri)), model);
        }
        if (signal?.aborted) return { cancelled: true };
        const roots = project.roots || (isFile ? [path.dirname(fileURLToPath(project.model.uri))] : []);
        for (const root of roots) await fs.mkdir(snapshotPath(root), { recursive: true });
        const rootArgs = roots.flatMap(root => ['-I', snapshotPath(root)]);
        // -S runs parsing/type checking without linking or running a user program.
        // All generated assembly stays inside the disposable snapshot.
        const execution = await new Promise(resolve => {
            execFile(command, [entry, '-S', '-o', path.join(directory, 'check.s'), ...rootArgs],
                { cwd: directory, encoding: 'utf8', windowsHide: true, timeout: 10000, maxBuffer: 1024 * 1024, signal },
                (error, stdout, stderr) => resolve({ error, stderr }));
        });
        if (signal?.aborted) return { cancelled: true };
        const diagnostics = parseDiagnostics(execution.stderr, files);
        let issue;
        if (execution.error && !diagnostics.some(d => d.severity === 'error')) {
            issue = execution.error.code === 'ENOENT'
                ? 'qyc が見つかりません。QYNO 0.27.9 のコンパイラをインストールし、qyno.compilerPath に実行ファイルを指定してください。'
                : `qyc の検査を完了できません: ${execution.stderr.trim() || execution.error.message}`;
        }
        return { diagnostics, issue };
    } finally {
        await fs.rm(directory, { recursive: true, force: true });
    }
}

function registerDiagnostics(vscode, context, index) {
    const collection = vscode.languages.createDiagnosticCollection('qyno');
    let timer, controller, revision = 0, disposed = false;
    const eligible = document => document?.languageId === 'qyno' && ['file', 'untitled'].includes(document.uri.scheme);
    const cancel = () => { clearTimeout(timer); controller?.abort(); revision++; };
    const validate = async document => {
        cancel();
        const generation = revision;
        collection.clear();
        if (disposed || !eligible(document) || !vscode.workspace.isTrusted) return;
        const config = vscode.workspace.getConfiguration('qyno', document.uri);
        if (!config.get('diagnostics.enabled', true)) return;
        controller = new AbortController();
        const signal = controller.signal;
        const publishIssue = message => {
            const diagnostic = new vscode.Diagnostic(new vscode.Range(0, 0, 0, 0), message, vscode.DiagnosticSeverity.Warning);
            diagnostic.source = 'QYNO'; collection.set(document.uri, [diagnostic]);
        };
        try {
            const folder = index.base(document).fsPath;
            const entrySetting = config.get('entryFile', '');
            const entry = entrySetting ? vscode.Uri.file(path.resolve(folder, entrySetting)) : document.uri;
            const entryModel = await index.model(entry);
            const project = await index.project({ uri: document.uri, getText: () => entryModel.lines.join('\n') }, false);
            project.model.uri = entry.toString();
            let command = config.get('compilerPath', 'qyc').trim() || 'qyc';
            if (!path.isAbsolute(command) && /[\\/]/.test(command)) command = path.resolve(folder, command);
            const result = await checkProject(command, project, signal);
            if (disposed || revision !== generation || result.cancelled) return;
            const grouped = new Map();
            for (const item of result.diagnostics) {
                const diagnostic = new vscode.Diagnostic(new vscode.Range(item.line, item.start, item.line, item.end), item.message,
                    item.severity === 'error' ? vscode.DiagnosticSeverity.Error : vscode.DiagnosticSeverity.Warning);
                diagnostic.source = 'QYNO';
                if (!grouped.has(item.uri)) grouped.set(item.uri, []);
                grouped.get(item.uri).push(diagnostic);
            }
            for (const [uri, diagnostics] of grouped) collection.set(vscode.Uri.parse(uri), diagnostics);
            if (result.issue) publishIssue(result.issue);
        } catch (error) {
            if (!disposed && revision === generation && !signal.aborted) publishIssue(`QYNO の検査を開始できません: ${error.message}`);
        }
    };
    const schedule = document => {
        cancel(); collection.clear();
        if (!eligible(document)) return;
        const delay = vscode.workspace.getConfiguration('qyno', document.uri).get('diagnostics.delay', 500);
        timer = setTimeout(() => validate(document), delay);
    };
    context.subscriptions.push(collection,
        { dispose() { disposed = true; cancel(); } },
        vscode.workspace.onDidOpenTextDocument(document => { if (eligible(document)) schedule(document); }),
        vscode.workspace.onDidChangeTextDocument(event => { if (eligible(event.document)) schedule(event.document); }),
        vscode.workspace.onDidSaveTextDocument(document => { if (eligible(document)) schedule(document); }),
        vscode.window.onDidChangeActiveTextEditor(editor => schedule(editor?.document)),
        vscode.workspace.onDidCloseTextDocument(document => {
            collection.delete(document.uri);
            if (eligible(document)) { cancel(); collection.clear(); schedule(vscode.window.activeTextEditor?.document); }
        }),
        vscode.workspace.onDidChangeConfiguration(event => { if (event.affectsConfiguration('qyno')) schedule(vscode.window.activeTextEditor?.document); }),
        vscode.workspace.onDidGrantWorkspaceTrust(() => schedule(vscode.window.activeTextEditor?.document)),
        vscode.commands.registerCommand('qyno.checkDocument', () => validate(vscode.window.activeTextEditor?.document)));
    schedule(vscode.window.activeTextEditor?.document);
    return { schedule };
}

module.exports = { utf16Column, parseDiagnostics, checkProject, registerDiagnostics };
