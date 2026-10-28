'use strict';
const vscode = require('vscode');
const path = require('node:path');
const os = require('node:os');
const { analyze, typeLabel, functionLabel } = require('./analysis');
const language = require('./language');
const { registerDiagnostics } = require('./diagnostics');

class Index {
    constructor() { this.cache = new Map(); }
    directory(document) {
        return document.uri.scheme === 'file' ? document.uri.with({ path: document.uri.path.slice(0, document.uri.path.lastIndexOf('/')) })
            : vscode.workspace.workspaceFolders?.[0]?.uri || vscode.Uri.file(os.tmpdir());
    }
    base(document) { return vscode.workspace.getWorkspaceFolder(document.uri)?.uri || this.directory(document); }
    async model(uri) {
        const open = vscode.workspace.textDocuments.find(d => d.uri.toString() === uri.toString());
        if (open) return analyze(open.getText(), uri.toString());
        const key = uri.toString();
        if (!this.cache.has(key)) this.cache.set(key, analyze(new TextDecoder().decode(await vscode.workspace.fs.readFile(uri)), key));
        return this.cache.get(key);
    }
    roots(document) {
        const config = vscode.workspace.getConfiguration('qyno', document.uri);
        const directory = uri => uri.with({ path: uri.path.slice(0, uri.path.lastIndexOf('/')) });
        const relative = value => path.isAbsolute(value) ? vscode.Uri.file(value) : vscode.Uri.joinPath(this.base(document), value);
        const entry = config.get('entryFile', '');
        return [entry ? directory(relative(entry)) : this.directory(document), ...config.get('moduleRoots', []).map(relative)];
    }
    async project(document, discover, token) {
        const model = analyze(document.getText(), document.uri.toString());
        const roots = this.roots(document), modules = new Map(), moduleNames = [];
        const visited = new Set();
        const loadImports = async source => {
            for (const item of source.imports) {
                if (token?.isCancellationRequested || visited.size >= 2000) return;
                if (visited.has(item.path)) continue;
                visited.add(item.path);
                for (const root of roots) {
                    try {
                        const imported = await this.model(vscode.Uri.joinPath(root, ...item.path.split('.').slice(0, -1), item.path.split('.').at(-1) + '.qy'));
                        modules.set(item.path, imported);
                        await loadImports(imported); break;
                    } catch { /* Unfinished or missing modules are normal while editing. */ }
                }
            }
        };
        await loadImports(model);
        if (discover) {
            const limit = vscode.workspace.getConfiguration('qyno', document.uri).get('maxIndexedFiles', 2000);
            let visited = 0, scanned = 0;
            const walk = async (uri, prefix = []) => {
                if (visited >= limit || scanned >= limit * 10 || token?.isCancellationRequested) return;
                let entries; try { entries = await vscode.workspace.fs.readDirectory(uri); } catch { return; }
                for (const [name, type] of entries.sort(([a], [b]) => a.localeCompare(b))) {
                    if (visited >= limit || scanned++ >= limit * 10 || token?.isCancellationRequested) break;
                    if (type & vscode.FileType.SymbolicLink) continue;
                    if (type === vscode.FileType.Directory && /^[A-Za-z_]\w*$/.test(name) && !['node_modules', '__pycache__'].includes(name)) await walk(vscode.Uri.joinPath(uri, name), [...prefix, name]);
                    if (type === vscode.FileType.File && /^[A-Za-z_]\w*\.qy$/.test(name)) { visited++; moduleNames.push([...prefix, name.slice(0, -3)].join('.')); }
                }
            };
            for (const root of roots) await walk(root);
        }
        return { model, modules, moduleNames, roots: roots.map(root => root.fsPath) };
    }
}
const kinds = { interface: vscode.CompletionItemKind.Interface, model: vscode.CompletionItemKind.Class, field: vscode.CompletionItemKind.Field, function: vscode.CompletionItemKind.Function, variable: vscode.CompletionItemKind.Variable, parameter: vscode.CompletionItemKind.Variable, constant: vscode.CompletionItemKind.Constant, namespace: vscode.CompletionItemKind.Module, property: vscode.CompletionItemKind.Property, type: vscode.CompletionItemKind.TypeParameter, keyword: vscode.CompletionItemKind.Keyword };
function markdown(symbol) {
    const result = new vscode.MarkdownString();
    result.appendCodeblock(['model', 'interface'].includes(symbol.kind) ? `${symbol.moduleVisibility?symbol.moduleVisibility+' ':''}${symbol.kind} ${symbol.name}` : symbol.kind === 'function' ? functionLabel(symbol, symbol.path || symbol.name) : symbol.type ? typeLabel(symbol) : symbol.path || symbol.name, 'qyno');
    if (symbol.documentation) result.appendText('\n' + symbol.documentation);
    return result;
}
function activate(context) {
    const index = new Index(), selector = { language: 'qyno' };
    const watch = vscode.workspace.createFileSystemWatcher('**/*.qy');
    context.subscriptions.push(watch, watch.onDidChange(uri => index.cache.delete(uri.toString())), watch.onDidDelete(uri => index.cache.delete(uri.toString())));
    const diagnostics = registerDiagnostics(vscode, context, index);
    context.subscriptions.push(watch.onDidChange(() => diagnostics.schedule(vscode.window.activeTextEditor?.document)),
        watch.onDidCreate(() => { index.cache.clear(); diagnostics.schedule(vscode.window.activeTextEditor?.document); }),
        watch.onDidDelete(() => diagnostics.schedule(vscode.window.activeTextEditor?.document)));
    context.subscriptions.push(vscode.languages.registerCompletionItemProvider(selector, {
        async provideCompletionItems(document, position, token) {
            const discover = /^\s*(import|using)\s/.test(document.lineAt(position.line).text);
            const project = await index.project(document, discover, token);
            return language.complete(project, position.line, position.character).map(item => {
                const result = new vscode.CompletionItem(item.label, kinds[item.kind]);
                result.detail = item.detail; result.documentation = item.documentation;
                result.insertText = item.snippet ? new vscode.SnippetString(item.insertText) : item.insertText;
                result.range = new vscode.Range(position.line, item.range.start, position.line, item.range.end);
                return result;
            });
        }
    }, '.', ':'));
    context.subscriptions.push(vscode.languages.registerHoverProvider(selector, {
        async provideHover(document, position, token) {
            const project = await index.project(document, false, token);
            const symbol = language.symbolAt(project, position.line, position.character);
            return symbol ? new vscode.Hover(markdown(symbol)) : undefined;
        }
    }));
    context.subscriptions.push(vscode.languages.registerDefinitionProvider(selector, {
        async provideDefinition(document, position, token) {
            const project = await index.project(document, false, token);
            const symbol = language.symbolAt(project, position.line, position.character);
            if (symbol?.uri) return new vscode.Location(vscode.Uri.parse(symbol.uri), new vscode.Range(symbol.line, symbol.start, symbol.line, symbol.end));
            const clause = project.model.imports.find(i => i.line === position.line && position.character >= i.start && position.character <= i.end);
            const target = clause && project.modules.get(clause.path);
            if (target) return new vscode.Location(vscode.Uri.parse(target.uri), new vscode.Position(0, 0));
        }
    }));
    context.subscriptions.push(vscode.languages.registerSignatureHelpProvider(selector, {
        async provideSignatureHelp(document, position, token) {
            const project = await index.project(document, false, token);
            const found = language.signature(project, position.line, position.character);
            if (!found) return undefined;
            const help = new vscode.SignatureHelp(), signature = new vscode.SignatureInformation(found.label, found.symbol.documentation);
            signature.parameters = found.symbol.params.map(p => new vscode.ParameterInformation(typeLabel(p)));
            help.signatures = [signature]; help.activeSignature = 0; help.activeParameter = found.activeParameter;
            return help;
        }
    }, '(', ',', '='));
    context.subscriptions.push(vscode.languages.registerDocumentSymbolProvider(selector, {
        provideDocumentSymbols(document) {
            const model = analyze(document.getText(), document.uri.toString());
            const outline = s => {
                const selection = new vscode.Range(s.line, s.start, s.line, s.end);
                const end = s.bodyScopeId ? model.scopes[s.bodyScopeId].endLine : s.line;
                const range = new vscode.Range(s.line, 0, end, model.lines[end].length);
                const symbol = new vscode.DocumentSymbol(s.name, s.kind === 'function' ? functionLabel(s) : typeLabel(s), s.kind === 'interface' ? vscode.SymbolKind.Interface : s.kind === 'model' ? vscode.SymbolKind.Struct : s.initializer ? vscode.SymbolKind.Constructor : s.kind === 'function' ? vscode.SymbolKind.Function : s.kind === 'constant' ? vscode.SymbolKind.Constant : vscode.SymbolKind.Variable, range, selection);
                if (['model', 'interface'].includes(s.kind)) symbol.children = s.members.map(outline);
                return symbol;
            };
            return model.symbols.filter(s => s.scopeId === 0).map(outline);
        }
    }));
}
module.exports = { activate, Index };
