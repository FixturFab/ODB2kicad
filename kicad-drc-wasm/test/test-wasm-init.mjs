/**
 * Debug WASM init crash: intercept all output and abort signals.
 */
import { dirname, resolve } from 'path';
import { fileURLToPath } from 'url';

const __dirname = dirname(fileURLToPath(import.meta.url));
const mjsPath = resolve(__dirname, '../build-wasm/kicad_drc.mjs');

console.log('Loading WASM module...');

try {
    const { default: createKicadDRC } = await import(mjsPath);

    const Module = await createKicadDRC({
        print: (text) => console.log('[stdout]', text),
        printErr: (text) => console.error('[stderr]', text),
        onAbort: (what) => {
            console.error('=== onAbort called ===');
            console.error('Reason:', what);
            console.trace();
        },
        locateFile: (path) => resolve(__dirname, '../build-wasm/', path),
    });

    console.log('WASM module loaded successfully!');
    console.log('Exported functions:', Object.keys(Module).filter(k => k.startsWith('_kicad') || k.startsWith('_diag')));
    
    // Try diag_ping
    console.log('diag_ping result:', Module._diag_ping());

} catch (e) {
    console.error('=== Exception during module init ===');
    console.error('Type:', e.constructor.name);
    console.error('Message:', e.message);
    
    // The abort message might be in the error itself
    if (e.message && e.message.includes('Aborted')) {
        console.error('Module was aborted during initialization');
    }
    
    // Check if this is a RuntimeError (typically from WASM trap/unreachable)
    if (e instanceof WebAssembly.RuntimeError) {
        console.error('WebAssembly RuntimeError - likely abort() or __builtin_trap()');
    }
}
