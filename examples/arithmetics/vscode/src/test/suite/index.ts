import * as assert from 'node:assert/strict';
import * as fs from 'node:fs/promises';
import * as os from 'node:os';
import * as path from 'node:path';
import * as vscode from 'vscode';

async function waitFor<T>(producer: () => T | undefined, predicate: (value: T) => boolean, timeoutMs = 15000): Promise<T> {
    const deadline = Date.now() + timeoutMs;
    while (Date.now() < deadline) {
        const value = producer();
        if (value !== undefined && predicate(value)) {
            return value;
        }
        await new Promise(resolve => setTimeout(resolve, 200));
    }
    throw new Error('Timed out waiting for VSCode condition.');
}

export async function run(): Promise<void> {
    const tempDir = await fs.mkdtemp(path.join(os.tmpdir(), 'pegium-arithmetics-'));
    const filePath = path.join(tempDir, 'normalizable.calc');
    await fs.writeFile(
        filePath,
        '/**the best module*/\n'
        + 'Module basicMath\n'
        + '\n'
        + '/** test */\n'
        + 'def c: 1 + 2;\n'
        + '\n'
        + 'c;\n',
        'utf8'
    );

    const document = await vscode.workspace.openTextDocument(vscode.Uri.file(filePath));
    await vscode.window.showTextDocument(document);

    const diagnostics = await waitFor(
        () => vscode.languages.getDiagnostics(document.uri),
        values => values.some(diagnostic => diagnostic.code === 'arithmetics.expression-normalizable')
    );

    const actions = await vscode.commands.executeCommand<(vscode.Command | vscode.CodeAction)[]>(
        'vscode.executeCodeActionProvider',
        document.uri,
        new vscode.Range(new vscode.Position(4, 7), new vscode.Position(4, 12))
    );

    assert.ok(actions && actions.some(action => 'title' in action && action.title.includes('Replace with constant')));

    const completion = await vscode.commands.executeCommand<vscode.CompletionList>(
        'vscode.executeCompletionItemProvider',
        document.uri,
        new vscode.Position(6, 1)
    );
    assert.ok(completion.items.some(item => item.label === 'c'));

    const moduleHover = await vscode.commands.executeCommand<vscode.Hover[]>(
        'vscode.executeHoverProvider',
        document.uri,
        new vscode.Position(1, 8)
    );
    assert.ok(moduleHover && moduleHover.some(item => item.contents.some(content =>
        typeof content === 'string'
            ? content.includes('the best module')
            : 'value' in content && String(content.value).includes('the best module')
    )));

    const definitionHover = await vscode.commands.executeCommand<vscode.Hover[]>(
        'vscode.executeHoverProvider',
        document.uri,
        new vscode.Position(4, 4)
    );
    assert.ok(definitionHover && definitionHover.some(item => item.contents.some(content =>
        typeof content === 'string'
            ? content.includes('test')
            : 'value' in content && String(content.value).includes('test')
    )));

    const referenceHover = await vscode.commands.executeCommand<vscode.Hover[]>(
        'vscode.executeHoverProvider',
        document.uri,
        new vscode.Position(6, 0)
    );
    assert.ok(referenceHover && referenceHover.some(item => item.contents.some(content =>
        typeof content === 'string'
            ? content.includes('test')
            : 'value' in content && String(content.value).includes('test')
    )));

    assert.ok(diagnostics.length > 0);
}
