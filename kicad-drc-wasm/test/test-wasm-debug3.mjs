/**
 * Debug test: intercept abort and __assert_fail to find crash source.
 */
import { dirname, resolve } from 'path';
import { fileURLToPath } from 'url';

const __dirname = dirname(fileURLToPath(import.meta.url));
const mjsPath = resolve(__dirname, '../build-wasm/kicad_drc.mjs');

console.log('Loading WASM module with abort/assert interception...');

try {
    const { default: createKicadDRC } = await import(mjsPath);

    const Module = await createKicadDRC({
        print: (text) => console.log('[stdout]', text),
        printErr: (text) => console.error('[stderr]', text),
        onAbort: (what) => {
            console.error('=== ABORT called ===');
            console.error('Reason:', what);
            console.error('Stack:', new Error().stack);
        },
        // Override locateFile to ensure .wasm can be found
        locateFile: (path) => {
            return resolve(__dirname, '../build-wasm/', path);
        },
    });

    console.log('WASM module loaded successfully!');
    console.log('Exported kicad_ functions:',
        Object.keys(Module).filter(k => k.startsWith('_kicad') || k.startsWith('_diag')));
} catch (e) {
    console.error('=== CRASH ===');
    console.error('Type:', e.constructor.name);
    console.error('Message:', e.message);
    if (e.stack) {
        const lines = e.stack.split('\n');
        for (const line of lines) {
            console.error('  ', line.trim());
        }
    }
}
