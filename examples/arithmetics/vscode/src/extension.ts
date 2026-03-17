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

function serverExecutableName(): string {
    return process.platform === 'win32'
        ? 'pegium-example-arithmetics-lsp.exe'
        : 'pegium-example-arithmetics-lsp';
}

function addCandidate(candidates: string[], value: string | undefined): void {
    if (!value) {
        return;
    }
    const trimmed = value.trim();
    if (!trimmed) {
        return;
    }
    candidates.push(trimmed);
}

function resolveServerCommand(context: vscode.ExtensionContext): string | undefined {
    const executable = serverExecutableName();
    const candidates: string[] = [];

    const configured = vscode.workspace
        .getConfiguration()
        .get<string>('pegium.arithmetics.serverPath');
    addCandidate(candidates, configured);
    addCandidate(candidates, process.env.PEGIUM_ARITHMETICS_SERVER);

    for (const folder of vscode.workspace.workspaceFolders ?? []) {
        const root = folder.uri.fsPath;
        addCandidate(candidates, path.join(root, 'build', 'examples', 'arithmetics', executable));
        addCandidate(candidates, path.join(root, '..', 'build', 'examples', 'arithmetics', executable));
        addCandidate(candidates, path.join(root, '..', '..', 'build', 'examples', 'arithmetics', executable));
    }

    addCandidate(candidates, path.join(context.extensionPath, '..', '..', 'build', 'examples', 'arithmetics', executable));
    addCandidate(candidates, path.join(context.extensionPath, '..', '..', '..', 'build', 'examples', 'arithmetics', executable));
    addCandidate(candidates, executable);

    const deduped = Array.from(new Set(candidates));
    for (const candidate of deduped) {
        if (!candidate.includes('/') && !candidate.includes('\\')) {
            return candidate;
        }
        if (fs.existsSync(candidate)) {
            return path.resolve(candidate);
        }
    }

    return undefined;
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
        throw new Error('Unable to allocate a localhost port for the Pegium Arithmetics server.');
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
        channel.appendLine(`Pegium Arithmetics server exited (code=${code ?? 'null'}, signal=${signal ?? 'null'})`);
        if (serverProcess === process) {
            serverProcess = undefined;
        }
    });
    process.on('error', (error) => {
        channel.appendLine(`Pegium Arithmetics server process error: ${error.message}`);
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
            throw new Error(`Pegium Arithmetics server exited before accepting socket connections (code=${process.exitCode}).`);
        }

        try {
            const socket = await connectToSocket(port);
            channel.appendLine(`Connected to Pegium Arithmetics server on 127.0.0.1:${port}`);
            return { reader: socket, writer: socket };
        } catch {
            await delay(100);
        }
    }

    throw new Error(`Timed out while connecting to Pegium Arithmetics server on 127.0.0.1:${port}.`);
}

function createServerOptions(serverCommand: string, channel: vscode.OutputChannel): ServerOptions {
    return async () => {
        stopServerProcess();

        const port = await allocatePort();
        channel.appendLine(`Starting Pegium Arithmetics server: ${serverCommand} --port=${port}`);

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
            'Unable to locate pegium-example-arithmetics-lsp. Build the project and/or set pegium.arithmetics.serverPath.'
        );
        return;
    }

    outputChannel = vscode.window.createOutputChannel('Pegium Arithmetics');
    context.subscriptions.push(outputChannel);
    context.subscriptions.push(new vscode.Disposable(() => {
        stopServerProcess();
    }));

    const serverOptions = createServerOptions(serverCommand, outputChannel);
    const clientOptions: LanguageClientOptions = {
        documentSelector: [{ scheme: 'file', language: 'arithmetics' }],
        outputChannel
    };

    client = new LanguageClient(
        'pegium-arithmetics',
        'Pegium Arithmetics',
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
