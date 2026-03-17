import * as assert from 'node:assert/strict';
import * as fs from 'node:fs/promises';
import * as os from 'node:os';
import * as path from 'node:path';
import * as vscode from 'vscode';

async function waitForDiagnostics(uri: vscode.Uri, timeoutMs = 15000): Promise<vscode.Diagnostic[]> {
    const deadline = Date.now() + timeoutMs;
    while (Date.now() < deadline) {
        const diagnostics = vscode.languages.getDiagnostics(uri);
        if (diagnostics.length >= 2) {
            return diagnostics;
        }
        await new Promise(resolve => setTimeout(resolve, 200));
    }
    throw new Error('Timed out waiting for statemachine diagnostics.');
}

export async function run(): Promise<void> {
    const tempDir = await fs.mkdtemp(path.join(os.tmpdir(), 'pegium-statemachine-'));
    const filePath = path.join(tempDir, 'broken.statemachine');
    await fs.writeFile(filePath, 'statemachine TrafficLight\nevents tick tick\ninitialState red\nstate red\nend\nstate Red\nend\n', 'utf8');

    const document = await vscode.workspace.openTextDocument(vscode.Uri.file(filePath));
    await vscode.window.showTextDocument(document);

    const diagnostics = await waitForDiagnostics(document.uri);
    assert.ok(diagnostics.some(diagnostic => diagnostic.message.includes('capital letter')));
    assert.ok(diagnostics.some(diagnostic => diagnostic.message.includes('Duplicate identifier name: tick')));
}
