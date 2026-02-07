/**
 * Diagnostic test: identify the exact crash during WASM module loading.
 */
import { readFileSync } from 'fs';
import { createRequire } from 'module';
import { fileURLToPath } from 'url';
import { dirname, resolve } from 'path';

const __dirname = dirname(fileURLToPath(import.meta.url));
const wasmPath = resolve(__dirname, '../build-wasm/kicad_drc.mjs');

console.log('Loading WASM module from:', wasmPath);

try {
    const { default: createKicadDRC } = await import(wasmPath);

    const Module = await createKicadDRC({
        onAbort: (what) => {
            console.error('=== ABORT called ===');
            console.error('Reason:', what);
            console.error('Stack:', new Error().stack);
        },
        print: (text) => {
            console.log('[stdout]', text);
        },
        printErr: (text) => {
            console.error('[stderr]', text);
        }
    });

    console.log('WASM module loaded successfully!');
    console.log('Exported functions:', Object.keys(Module).filter(k => k.startsWith('_kicad')));
} catch (e) {
    console.error('=== WASM LOAD FAILED ===');
    console.error('Error type:', e.constructor.name);
    console.error('Error message:', e.message);
    console.error('Stack:', e.stack);
}
