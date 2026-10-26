'use strict';
const assert = require('node:assert/strict');
const path = require('node:path');
const vscode = require('vscode');
async function run() {
    let checks = 0;
    const extension = vscode.extensions.getExtension('hiraken0427.qyno-vscode');
    assert.ok(extension); await extension.activate(); checks++;
    const uri = vscode.Uri.file(path.join(__dirname,'workspace/main.qy'));
    const document = await vscode.workspace.openTextDocument(uri);
    const editor = await vscode.window.showTextDocument(document);
    assert.equal(document.languageId,'qyno'); checks++;
    const completion = async (text) => {
        const line = 8, range = document.lineAt(line).range;
        await editor.edit(edit => edit.replace(range, text));
        const position = new vscode.Position(line,text.length);
        editor.selection = new vscode.Selection(position, position);
        const result = await vscode.commands.executeCommand('vscode.executeCompletionItemProvider',uri,position);
        return result.items;
    };
    let items = await completion('\tmat');
    assert.ok(items.some(i=>i.label==='matrix')); checks++;
    items = await completion('\tfs.'); assert.ok(items.some(i=>i.label==='readline')); checks++;
    items = await completion('\tcircle.'); assert.ok(items.some(i=>i.label==='area')); checks++;
    items = await completion('\tmatrix[0].'); assert.ok(items.some(i=>i.label==='length')); checks++;
    items = await completion('\tio.input("入力> {mat'); assert.ok(items.some(i=>i.label==='matrix')); checks++;
    await completion('\tcircle.area(2.0)');
    const definitions = await vscode.commands.executeCommand('vscode.executeDefinitionProvider',uri,new vscode.Position(8,12));
    assert.ok(definitions.some(d=>(d.uri || d.targetUri).path.endsWith('/geometry/circle.qy'))); checks++;
    const hovers = await vscode.commands.executeCommand('vscode.executeHoverProvider',uri,new vscode.Position(8,12));
    assert.ok(hovers.length); checks++;
    await completion('\tfs.readline(source, ');
    const help = await vscode.commands.executeCommand('vscode.executeSignatureHelpProvider',uri,new vscode.Position(8,document.lineAt(8).text.length),'(',);
    assert.equal(help.activeParameter,1); assert.match(help.signatures[0].label,/ref str line/); checks++;
    const symbols = await vscode.commands.executeCommand('vscode.executeDocumentSymbolProvider',uri);
    assert.ok(symbols.some(s=>s.name==='main')); checks++;
    // Unsaved edits in both the active file and imported modules must be reflected.
    const target = await vscode.workspace.openTextDocument(vscode.Uri.file(path.join(__dirname,'workspace/geometry/circle.qy')));
    const edit = new vscode.WorkspaceEdit(); edit.insert(target.uri, new vscode.Position(target.lineCount-1,0),'double perimeter(double radius) = 2.0 * PI * radius\n');
    assert.ok(await vscode.workspace.applyEdit(edit));
    items = await completion('\tcircle.per'); assert.ok(items.some(i=>i.label==='perimeter')); checks++;
    const rename = new vscode.WorkspaceEdit(); rename.replace(uri,new vscode.Range(5,5,5,11),'grid');
    assert.ok(await vscode.workspace.applyEdit(rename));
    items = await completion('\tgri'); assert.ok(items.some(i=>i.label==='grid')); checks++;
    items = await completion('\tmat'); assert.ok(!items.some(i=>i.label==='matrix')); checks++;
    items = await completion('\t// fs.'); assert.ok(!items.some(i=>i.label==='readline')); checks++;
    console.log(`QYNO VS Code integration: ${checks} checks passed`);
}
module.exports={run};
