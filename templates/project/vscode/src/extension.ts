import * as childProcess from 'node:child_process';
import * as fs from 'node:fs';
import * as net from 'node:net';
import * as path from 'node:path';
import * as vscode from 'vscode';
import type { LanguageClientOptions, ServerOptions, StreamInfo } from 'vscode-languageclient/node';
import { LanguageClient } from 'vscode-languageclient/node';

let client: LanguageClient | undefined;
let outputChannel: vscode.OutputChannel | undefined;
let serverProcess: childProcess.ChildProcess | undefined;

function resolveServerCommand(context: vscode.ExtensionContext): string | undefined {
    const configured = vscode.workspace
        .getConfiguration()
        .get<string>('@PEGIUM_NEW_LANGUAGE_ID@.server.path');
    if (configured && configured.trim()) {
        return configured.trim();
    }

    const defaultName = process.platform === 'win32' ? '@PEGIUM_NEW_LANGUAGE_ID@-lsp.exe' : '@PEGIUM_NEW_LANGUAGE_ID@-lsp';

    // A packaged (published) extension ships the server next to it in bin/.
    const bundled = path.join(context.extensionPath, 'bin', defaultName);
    if (fs.existsSync(bundled)) {
        return makeExecutable(path.resolve(bundled));
    }

    // In development the server is built by CMake: single-config generators
    // (Ninja/Make) put it at the build root, multi-config ones (Visual Studio)
    // in a per-config subdirectory.
    const buildRoot = path.join(context.extensionPath, '..', 'build');
    for (const config of ['', 'Debug', 'Release', 'RelWithDebInfo']) {
        const candidate = path.join(buildRoot, config, defaultName);
        if (fs.existsSync(candidate)) {
            return path.resolve(candidate);
        }
    }

    return undefined;
}

// Packing into a .vsix can drop the executable bit, so restore it for the
// bundled server on POSIX platforms (no-op on Windows).
function makeExecutable(file: string): string {
    if (process.platform !== 'win32') {
        try {
            fs.chmodSync(file, 0o755);
        } catch {
            // best effort — the file may already be executable
        }
    }
    return file;
}

async function allocatePort(): Promise<number> {
    const probe = net.createServer();
    await new Promise<void>((resolve, reject) => {
        probe.once('error', reject);
        probe.listen(0, '127.0.0.1', () => resolve());
    });

    const address = probe.address();
    const port = typeof address === 'object' && address ? address.port : 0;
    await new Promise<void>((resolve, reject) => {
        probe.close((error) => {
            if (error) {
                reject(error);
                return;
            }
            resolve();
        });
    });

    if (!port) {
        throw new Error('Unable to allocate a localhost port for the @PEGIUM_NEW_CLASS@ server.');
    }
    return port;
}

function delay(milliseconds: number): Promise<void> {
    return new Promise((resolve) => {
        setTimeout(resolve, milliseconds);
    });
}

function stopServerProcess(): void {
    if (!serverProcess) {
        return;
    }
    const runningProcess = serverProcess;
    serverProcess = undefined;
    if (runningProcess.exitCode === null && !runningProcess.killed) {
        runningProcess.kill();
    }
}

function attachServerLogs(process: childProcess.ChildProcess, channel: vscode.OutputChannel): void {
    process.stdout?.on('data', (chunk: Buffer | string) => {
        channel.append(typeof chunk === 'string' ? chunk : chunk.toString('utf8'));
    });
    process.stderr?.on('data', (chunk: Buffer | string) => {
        channel.append(typeof chunk === 'string' ? chunk : chunk.toString('utf8'));
    });
    process.on('exit', (code, signal) => {
        channel.appendLine(`@PEGIUM_NEW_CLASS@ server exited (code=${code ?? 'null'}, signal=${signal ?? 'null'})`);
        if (serverProcess === process) {
            serverProcess = undefined;
        }
    });
    process.on('error', (error) => {
        channel.appendLine(`@PEGIUM_NEW_CLASS@ server process error: ${error.message}`);
        if (serverProcess === process) {
            serverProcess = undefined;
        }
    });
}

async function connectToSocket(port: number): Promise<net.Socket> {
    return await new Promise<net.Socket>((resolve, reject) => {
        const socket = net.createConnection({ host: '127.0.0.1', port });
        socket.once('connect', () => {
            socket.setNoDelay(true);
            resolve(socket);
        });
        socket.once('error', (error) => {
            socket.destroy();
            reject(error);
        });
    });
}

async function waitForServerSocket(
    port: number,
    process: childProcess.ChildProcess,
    channel: vscode.OutputChannel
): Promise<StreamInfo> {
    const deadline = Date.now() + 10_000;

    while (Date.now() < deadline) {
        if (process.exitCode !== null) {
            throw new Error(`@PEGIUM_NEW_CLASS@ server exited before accepting socket connections (code=${process.exitCode}).`);
        }

        try {
            const socket = await connectToSocket(port);
            channel.appendLine(`Connected to @PEGIUM_NEW_CLASS@ server on 127.0.0.1:${port}`);
            return { reader: socket, writer: socket };
        } catch {
            await delay(100);
        }
    }

    throw new Error(`Timed out while connecting to @PEGIUM_NEW_CLASS@ server on 127.0.0.1:${port}.`);
}

function createServerOptions(serverCommand: string, channel: vscode.OutputChannel): ServerOptions {
    return async () => {
        stopServerProcess();

        const port = await allocatePort();
        channel.appendLine(`Starting @PEGIUM_NEW_CLASS@ server: ${serverCommand} --port=${port}`);

        const child = childProcess.spawn(serverCommand, [`--port=${port}`], {
            cwd: path.dirname(serverCommand),
            env: process.env,
            stdio: ['ignore', 'pipe', 'pipe']
        });
        serverProcess = child;
        attachServerLogs(child, channel);

        try {
            return await waitForServerSocket(port, child, channel);
        } catch (error) {
            stopServerProcess();
            throw error;
        }
    };
}

export async function activate(context: vscode.ExtensionContext): Promise<void> {
    const serverCommand = resolveServerCommand(context);
    if (!serverCommand) {
        void vscode.window.showErrorMessage(
            'Unable to locate @PEGIUM_NEW_LANGUAGE_ID@-lsp. Build the project and/or set @PEGIUM_NEW_LANGUAGE_ID@.server.path.'
        );
        return;
    }

    outputChannel = vscode.window.createOutputChannel('@PEGIUM_NEW_CLASS@');
    context.subscriptions.push(outputChannel);
    context.subscriptions.push(new vscode.Disposable(() => {
        stopServerProcess();
    }));

    const serverOptions = createServerOptions(serverCommand, outputChannel);
    const clientOptions: LanguageClientOptions = {
        documentSelector: [{ scheme: 'file', language: '@PEGIUM_NEW_LANGUAGE_ID@' }],
        outputChannel
    };

    client = new LanguageClient(
        '@PEGIUM_NEW_LANGUAGE_ID@',
        '@PEGIUM_NEW_CLASS@',
        serverOptions,
        clientOptions
    );

    await client.start();
}

export async function deactivate(): Promise<void> {
    if (client) {
        const runningClient = client;
        client = undefined;
        await runningClient.stop();
    }
    stopServerProcess();
}
