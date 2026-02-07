/**
 * Debug: Try to understand the crash by examining the WASM module structure
 */
import { dirname, resolve } from 'path';
import { fileURLToPath } from 'url';
import { readFileSync } from 'fs';

const __dirname = dirname(fileURLToPath(import.meta.url));

// Read symbol map for decoding
const symbolMap = {};
const lines = readFileSync(resolve(__dirname, '../build-wasm/kicad_drc.mjs.symbols'), 'utf8').split('\n');
for (const line of lines) {
    const colon = line.indexOf(':');
    if (colon !== -1) {
        symbolMap[line.substring(0, colon)] = line.substring(colon + 1);
    }
}

// Search for potential crash callers around the crash offset
// The crash is at wasm-function[110]:0x46ceb
// Let's look at what's called near that offset
console.log('Looking for function 110 in symbol map:', symbolMap['110']);

// Also check functions near 110
for (let i = 108; i <= 115; i++) {
    if (symbolMap[String(i)]) {
        console.log(`  Function ${i}: ${symbolMap[String(i)]}`);
    }
}

console.log('\nTrying to load WASM binary and instantiate manually...');

const wasmPath = resolve(__dirname, '../build-wasm/kicad_drc.wasm');
const wasmBytes = readFileSync(wasmPath);

// Use WebAssembly.compile to get the module without instantiating
const module = await WebAssembly.compile(wasmBytes);

// Get imports and exports
const imports = WebAssembly.Module.imports(module);
const exports_list = WebAssembly.Module.exports(module);

console.log('\nModule imports (' + imports.length + '):');
for (const imp of imports) {
    console.log(`  ${imp.module}.${imp.name} (${imp.kind})`);
}

console.log('\nExported functions:');
for (const exp of exports_list) {
    if (exp.kind === 'function') {
        console.log(`  ${exp.name}`);
    }
}
