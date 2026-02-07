/**
 * Debug test: catch runtime error from static constructors with detailed trace.
 */
import { dirname, resolve } from 'path';
import { fileURLToPath } from 'url';

const __dirname = dirname(fileURLToPath(import.meta.url));
const mjsPath = resolve(__dirname, '../build-wasm/kicad_drc.mjs');

console.log('Loading WASM module...');

// Override process to catch uncaught exceptions with more detail
process.on('uncaughtException', (err) => {
    console.error('=== Uncaught Exception ===');
    console.error(err);
});

try {
    const { default: createKicadDRC } = await import(mjsPath);

    const Module = await createKicadDRC({
        print: (text) => console.log('[stdout]', text),
        printErr: (text) => console.error('[stderr]', text),
        onAbort: (what) => {
            console.error('=== ABORT ===');
            console.error('Reason:', what);
            console.error('Stack:', new Error().stack);
        },
    });

    console.log('WASM module loaded successfully!');
    console.log('Exported kicad_ functions:',
        Object.keys(Module).filter(k => k.startsWith('_kicad')));

    // Try calling diag_ping
    if (Module._diag_ping) {
        console.log('diag_ping result:', Module._diag_ping());
    }
} catch (e) {
    console.error('=== CRASH ===');
    console.error('Type:', e.constructor.name);
    console.error('Message:', e.message);
    if (e.stack) {
        console.error('Stack:', e.stack);
    }
}
