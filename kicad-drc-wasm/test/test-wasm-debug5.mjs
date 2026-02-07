/**
 * Debug: Try loading with abort override to see what causes the crash
 */
import { dirname, resolve } from 'path';
import { fileURLToPath } from 'url';

const __dirname = dirname(fileURLToPath(import.meta.url));
const mjsPath = resolve(__dirname, '../build-wasm/kicad_drc.mjs');

// Read the .mjs file to understand what functions it imports
import { readFileSync } from 'fs';
const mjsSrc = readFileSync(mjsPath, 'utf8');

// Check what the abort function looks like in the generated JS
const abortMatch = mjsSrc.match(/function\s+_abort_js[^}]+}/);
console.log('_abort_js function:', abortMatch ? abortMatch[0].substring(0, 200) : 'not found');

const { default: createKicadDRC } = await import(mjsPath);

try {
    const Module = await createKicadDRC({
        print: (text) => console.log('[out]', text),
        printErr: (text) => console.error('[err]', text),
        onAbort: (what) => {
            console.error('[ABORT] reason:', what);
            console.error('[ABORT] stack:', new Error().stack);
        },
        locateFile: (path) => resolve(__dirname, '../build-wasm/', path),
    });
    console.log('Module loaded!');
} catch (e) {
    console.error('\n=== Exception during module load ===');
    console.error('Type:', e.constructor.name);
    console.error('Message:', e.message);

    // If it's a RuntimeError with 'unreachable', this is abort() or __builtin_trap()
    if (e.message === 'unreachable') {
        console.error('\nThis is a WASM unreachable instruction (abort/trap).');
        console.error('The crash happened during static initialization.');
        console.error('Stack:', e.stack);
    }
}
