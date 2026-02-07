/**
 * Debug: intercept terminate to catch what exception fires during static init
 */
import { dirname, resolve } from 'path';
import { fileURLToPath } from 'url';
import { readFileSync } from 'fs';

const __dirname = dirname(fileURLToPath(import.meta.url));
const wasmBuf = readFileSync(resolve(__dirname, '../build-wasm/kicad_drc.wasm'));
const mjsPath = resolve(__dirname, '../build-wasm/kicad_drc.mjs');

// Read symbol map
const symbolMap = {};
const lines = readFileSync(resolve(__dirname, '../build-wasm/kicad_drc.mjs.symbols'), 'utf8').split('\n');
for (const line of lines) {
    const colon = line.indexOf(':');
    if (colon !== -1) {
        symbolMap[line.substring(0, colon)] = line.substring(colon + 1);
    }
}

const { default: createKicadDRC } = await import(mjsPath);

try {
    const Module = await createKicadDRC({
        print: (text) => console.log('[out]', text),
        printErr: (text) => {
            console.error('[err]', text);
        },
        onAbort: (what) => {
            console.error('=== ABORT ===');
            console.error('Reason:', what);
            throw new Error('Abort: ' + what);
        },
        locateFile: (path) => resolve(__dirname, '../build-wasm/', path),
    });
    console.log('Module loaded successfully!');
    console.log('Exported:', Object.keys(Module).filter(k => k.startsWith('_kicad') || k.startsWith('_diag')));
} catch (e) {
    console.error('=== EXCEPTION ===');
    console.error('Type:', e.constructor.name);
    console.error('Message:', e.message);
    
    // Parse stack trace to find WASM function indices
    const funcMatches = e.stack?.matchAll(/wasm-function\[(\d+)\]/g);
    if (funcMatches) {
        console.error('\nWASM call stack (decoded):');
        for (const m of funcMatches) {
            const idx = m[1];
            const name = symbolMap[idx] || '(unknown)';
            console.error(`  Function[${idx}] = ${name}`);
        }
    }
}
