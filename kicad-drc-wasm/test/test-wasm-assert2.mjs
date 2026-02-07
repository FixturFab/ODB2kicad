/**
 * Intercept __assert_fail to find what asserts during static init
 */
import { readFileSync } from 'fs';
import { dirname, resolve } from 'path';
import { fileURLToPath } from 'url';

const __dirname = dirname(fileURLToPath(import.meta.url));

// Read the raw wasm file
const wasmPath = resolve(__dirname, '../build-wasm/kicad_drc.wasm');
const mjsPath = resolve(__dirname, '../build-wasm/kicad_drc.mjs');

const { default: createKicadDRC } = await import(mjsPath);

try {
    const Module = await createKicadDRC({
        print: (text) => console.log('[stdout]', text),
        printErr: (text) => console.error('[stderr]', text),
        onAbort: (what) => {
            console.error('=== ABORT ===');
            console.error('Reason:', what);
            console.error('Stack:', new Error().stack);
        },
        locateFile: (path) => resolve(__dirname, '../build-wasm/', path),
    });
    
    console.log('Module loaded!');
} catch (e) {
    console.error('=== EXCEPTION ===');
    console.error(e.message);
    console.error(e.stack);
}
