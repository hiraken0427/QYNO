'use strict';
const path = require('node:path');
const { runTests } = require('@vscode/test-electron');
runTests({
    version: '1.85.2',
    ...(process.env.QYNO_VSCODE_EXECUTABLE ? { vscodeExecutablePath: process.env.QYNO_VSCODE_EXECUTABLE } : {}),
    extensionDevelopmentPath: path.resolve(__dirname, '..'),
    extensionTestsPath: path.resolve(__dirname, 'editor.js'),
    launchArgs: [path.resolve(__dirname, 'workspace'), '--no-sandbox', '--disable-gpu', '--disable-workspace-trust', '--skip-welcome', '--skip-release-notes']
}).catch(error => { console.error(error); process.exitCode = 1; });
