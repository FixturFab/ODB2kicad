#!/usr/bin/env node
// Build script: copies WASM artifacts and wrapper into dist/

import { cpSync, mkdirSync, existsSync } from 'fs';
import { join, dirname } from 'path';
import { fileURLToPath } from 'url';

const __dirname = dirname(fileURLToPath(import.meta.url));
const root = join(__dirname, '..');
const dist = join(root, 'dist');
const wasmBuild = join(root, '..', 'odb2kicad', 'build-wasm');

// Verify WASM build artifacts exist
const wasmFile = join(wasmBuild, 'odb2kicad_wasm.wasm');
const mjsFile = join(wasmBuild, 'odb2kicad_wasm.mjs');

if (!existsSync(wasmFile)) {
  console.error(`ERROR: WASM artifact not found: ${wasmFile}`);
  console.error('Run the Emscripten build first:');
  console.error('  source /root/emsdk/emsdk_env.sh');
  console.error('  emcmake cmake -S odb2kicad -B odb2kicad/build-wasm -DCMAKE_BUILD_TYPE=Release');
  console.error('  emmake make -C odb2kicad/build-wasm -j$(nproc)');
  process.exit(1);
}

// Create dist/
mkdirSync(dist, { recursive: true });

// Copy WASM artifacts
cpSync(wasmFile, join(dist, 'odb2kicad_wasm.wasm'));
cpSync(mjsFile, join(dist, 'odb2kicad_wasm.mjs'));

// Copy wrapper
cpSync(join(root, 'src', 'index.mjs'), join(dist, 'index.mjs'));
cpSync(join(root, 'src', 'index.d.ts'), join(dist, 'index.d.ts'));

console.log('Build complete:');
console.log('  dist/index.mjs          — JS wrapper');
console.log('  dist/index.d.ts         — TypeScript declarations');
console.log('  dist/odb2kicad_wasm.mjs — Emscripten JS glue');
console.log('  dist/odb2kicad_wasm.wasm — WebAssembly binary');
