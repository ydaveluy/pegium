import * as assert from 'node:assert/strict';
import * as fs from 'node:fs/promises';
import * as os from 'node:os';
import * as path from 'node:path';
import * as vscode from 'vscode';

export async function run(): Promise<void> {
    const tempDir = await fs.mkdtemp(path.join(os.tmpdir(), 'pegium-domainmodel-'));
    const filePath = path.join(tempDir, 'rename.dmodel');
    await fs.writeFile(filePath, 'package foo {\n  entity Person {\n  }\n}\nentity Holder {\n  person: foo.Person\n  many friends: Person\n}\n', 'utf8');

    const document = await vscode.workspace.openTextDocument(vscode.Uri.file(filePath));
    await vscode.window.showTextDocument(document);

    const edit = await vscode.commands.executeCommand<vscode.WorkspaceEdit>(
        'vscode.executeDocumentRenameProvider',
        document.uri,
        new vscode.Position(1, 9),
        'User'
    );

    assert.ok(edit);
    const entries = edit.entries();
    assert.ok(entries.length > 0);
    const edits = entries.flatMap(([, textEdits]) => textEdits);
    assert.ok(edits.some(textEdit => textEdit.newText === 'foo.User'));
    assert.ok(edits.some(textEdit => textEdit.newText === 'User'));
}
