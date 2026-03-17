import * as path from 'node:path';
import { runTests } from '@vscode/test-electron';

function serverExecutableName(): string {
    return process.platform === 'win32' ? 'pegium-example-requirements-lsp.exe' : 'pegium-example-requirements-lsp';
}

async function main(): Promise<void> {
    const extensionDevelopmentPath = path.resolve(__dirname, '../../..');
    const extensionTestsPath = path.resolve(__dirname, './suite/index');
    const repoRoot = path.resolve(extensionDevelopmentPath, '..', '..');
    const serverPath = path.join(repoRoot, 'build', 'examples', 'requirements', serverExecutableName());
    await runTests({
        extensionDevelopmentPath,
        extensionTestsPath,
        extensionTestsEnv: {
            ...process.env,
            PEGIUM_REQUIREMENTS_SERVER: serverPath
        },
        launchArgs: [extensionDevelopmentPath, '--disable-extensions']
    });
}

void main();
