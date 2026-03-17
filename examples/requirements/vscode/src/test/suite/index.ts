import * as assert from 'node:assert/strict';
import * as fs from 'node:fs/promises';
import * as os from 'node:os';
import * as path from 'node:path';
import * as vscode from 'vscode';

async function waitForDiagnostics(uri: vscode.Uri, needle: string, timeoutMs = 15000): Promise<vscode.Diagnostic[]> {
    const deadline = Date.now() + timeoutMs;
    while (Date.now() < deadline) {
        const diagnostics = vscode.languages.getDiagnostics(uri);
        if (diagnostics.some(diagnostic => diagnostic.message.includes(needle))) {
            return diagnostics;
        }
        await new Promise(resolve => setTimeout(resolve, 200));
    }
    throw new Error(`Timed out waiting for diagnostic containing: ${needle}`);
}

export async function run(): Promise<void> {
    const tempDir = await fs.mkdtemp(path.join(os.tmpdir(), 'pegium-requirements-'));

    const uncoveredReq = path.join(tempDir, 'uncovered.req');
    await fs.writeFile(uncoveredReq, 'environment dev: "Development"\nreq REQ "Uncovered requirement" applicable for dev\n', 'utf8');
    const uncoveredDocument = await vscode.workspace.openTextDocument(vscode.Uri.file(uncoveredReq));
    await vscode.window.showTextDocument(uncoveredDocument);
    const uncoveredDiagnostics = await waitForDiagnostics(uncoveredDocument.uri, 'not covered by a test');
    assert.ok(uncoveredDiagnostics.length > 0);

    const requirementsPath = path.join(tempDir, 'requirements.req');
    const testsPath = path.join(tempDir, 'tests.tst');
    await fs.writeFile(requirementsPath, 'environment dev: "Development"\nenvironment prod: "Production"\nreq REQ1 "Covered requirement" applicable for dev\n', 'utf8');
    await fs.writeFile(testsPath, 'tst TEST1 tests REQ1 applicable for prod\n', 'utf8');

    const requirementsDocument = await vscode.workspace.openTextDocument(vscode.Uri.file(requirementsPath));
    await vscode.window.showTextDocument(requirementsDocument);
    const testsDocument = await vscode.workspace.openTextDocument(vscode.Uri.file(testsPath));
    await vscode.window.showTextDocument(testsDocument);

    const mismatchDiagnostics = await waitForDiagnostics(testsDocument.uri, 'references environment prod');
    assert.ok(mismatchDiagnostics.length > 0);
}
