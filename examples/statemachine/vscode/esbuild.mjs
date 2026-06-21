import { rmSync } from 'node:fs';
import esbuild from 'esbuild';

const production = process.argv.includes('--production');
const watch = process.argv.includes('--watch');

// Start from a clean output dir so stale artifacts (e.g. a dev sourcemap, or
// test files compiled by tsc) never leak into a production bundle or the .vsix.
if (!watch) {
  rmSync('out', { recursive: true, force: true });
}

// Bundle the extension into a single CommonJS file. Only `vscode` is external
// (the editor injects it at runtime); everything else — including
// vscode-languageclient — is bundled in, so the packaged .vsix is self-contained.
const context = await esbuild.context({
  entryPoints: ['src/extension.ts'],
  bundle: true,
  format: 'cjs',
  platform: 'node',
  target: 'node18',
  outfile: 'out/extension.js',
  external: ['vscode'],
  sourcemap: !production,
  minify: production,
  logLevel: 'info',
});

if (watch) {
  await context.watch();
} else {
  await context.rebuild();
  await context.dispose();
}
