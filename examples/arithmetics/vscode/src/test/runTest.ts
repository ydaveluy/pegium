import * as path from 'node:path';
import { runTests } from '@vscode/test-electron';

function serverExecutableName(): string {
    return process.platform === 'win32' ? 'pegium-example-arithmetics-lsp.exe' : 'pegium-example-arithmetics-lsp';
}

async function main(): Promise<void> {
    const extensionDevelopmentPath = path.resolve(__dirname, '../../..');
    const extensionTestsPath = path.resolve(__dirname, './suite/index');
    const repoRoot = path.resolve(extensionDevelopmentPath, '..', '..');
    const serverPath = path.join(repoRoot, 'build', 'examples', 'arithmetics', serverExecutableName());
    await runTests({
        extensionDevelopmentPath,
        extensionTestsPath,
        extensionTestsEnv: {
            ...process.env,
            PEGIUM_ARITHMETICS_SERVER: serverPath
        },
        launchArgs: [extensionDevelopmentPath, '--disable-extensions']
    });
}

void main();
