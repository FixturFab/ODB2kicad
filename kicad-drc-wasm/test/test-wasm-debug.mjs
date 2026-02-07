/**
 * Debug test: identify static init crash in WASM DRC module.
 */
import createKicadDRC from '../build-wasm/kicad_drc.mjs';

console.log('About to load WASM module...');

try {
    const Module = await createKicadDRC({
        // Override abort to get better error info
        onAbort: (what) => {
            console.error('ABORT called with:', what);
        },
        // Print stdout/stderr
        print: (text) => {
            console.log('[WASM stdout]', text);
        },
        printErr: (text) => {
            console.error('[WASM stderr]', text);
        }
    });
    console.log('WASM module loaded successfully!');
} catch (e) {
    console.error('WASM load failed:', e.message);
    console.error('Stack:', e.stack);
}
