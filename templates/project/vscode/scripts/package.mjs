// Build an optimized, platform-specific .vsix that bundles the native server.
//
//   npm run package                       # for the host platform
//   npm run package -- --target linux-x64 # for a specific VS Code target
//   npm run publish                        # package + publish (needs VSCE_PAT)
//
// The server is built Release and stripped, in a dedicated tree kept apart from
// the Debug build/ that F5 uses, so the bundled binary is small and fast. The
// C++ server can rarely be cross-compiled, so each platform's .vsix is normally
// built on that platform's CI runner — see .github/workflows/release.yml.
import { execFileSync } from 'node:child_process';
import { chmodSync, copyFileSync, existsSync, mkdirSync, readFileSync, statSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { createVSIX, publishVSIX } from '@vscode/vsce';

const extensionDir = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const projectRoot = path.resolve(extensionDir, '..');
const buildDir = path.join(projectRoot, 'build-release');
const pkg = JSON.parse(readFileSync(path.join(extensionDir, 'package.json'), 'utf8'));

const serverName = '@PEGIUM_NEW_LANGUAGE_ID@-lsp';
const isWindows = process.platform === 'win32';
const exe = isWindows ? `${serverName}.exe` : serverName;

const argv = process.argv.slice(2);
const doPublish = argv.includes('--publish');
const ti = argv.indexOf('--target');
const target = ti !== -1 ? argv[ti + 1] : hostTarget();

function hostTarget() {
    const os = isWindows ? 'win32' : process.platform === 'darwin' ? 'darwin' : 'linux';
    const arch =
        process.arch === 'x64' ? 'x64' : process.arch === 'arm64' ? 'arm64' : process.arch === 'arm' ? 'armhf' : process.arch;
    return `${os}-${arch}`;
}

function run(cmd, args) {
    execFileSync(cmd, args, { stdio: 'inherit' });
}

// Build the server Release. -O2/NDEBUG make it fast; on POSIX, splitting into
// per-function sections lets the linker garbage-collect unused code.
console.log(`Building ${serverName} (Release)...`);
const configure = ['-S', projectRoot, '-B', buildDir, '-DCMAKE_BUILD_TYPE=Release'];
if (!isWindows) {
    configure.push('-DCMAKE_CXX_FLAGS=-ffunction-sections -fdata-sections');
    configure.push(`-DCMAKE_EXE_LINKER_FLAGS=${process.platform === 'darwin' ? '-Wl,-dead_strip' : '-Wl,--gc-sections'}`);
}
run('cmake', configure);
run('cmake', ['--build', buildDir, '--config', 'Release', '--target', serverName, '-j']);

let server;
for (const config of ['', 'Release', 'RelWithDebInfo']) {
    const candidate = path.join(buildDir, config, exe);
    if (existsSync(candidate)) {
        server = candidate;
        break;
    }
}
if (!server) {
    console.error(`Build did not produce ${exe}.`);
    process.exit(1);
}

// Bundle the server inside the extension so the .vsix is self-contained.
const binDir = path.join(extensionDir, 'bin');
mkdirSync(binDir, { recursive: true });
const dest = path.join(binDir, exe);
copyFileSync(server, dest);
if (!isWindows) {
    chmodSync(dest, 0o755);
    // Stripping symbols is the dominant size win (the Windows .exe keeps them in
    // a separate .pdb that we do not ship, so it is already lean).
    try {
        run('strip', [dest]);
    } catch {
        console.warn('strip not found — shipping an unstripped server');
    }
}
console.log(`Bundled ${exe}: ${(statSync(dest).size / 1048576).toFixed(1)} MB`);

const vsix = path.join(extensionDir, `${pkg.name}-${target}-${pkg.version}.vsix`);
await createVSIX({ cwd: extensionDir, target, packagePath: vsix, dependencies: false, allowMissingRepository: true });
console.log(`Packaged ${path.relative(projectRoot, vsix)} (target ${target})`);

if (doPublish) {
    await publishVSIX(vsix, { target });
    console.log(`Published ${target}`);
}
