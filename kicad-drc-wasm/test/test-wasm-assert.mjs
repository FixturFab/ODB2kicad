/**
 * Test WASM loading with assertion interception.
 */
import { readFileSync } from 'fs';
import { dirname, resolve } from 'path';
import { fileURLToPath } from 'url';

const __dirname = dirname(fileURLToPath(import.meta.url));
const mjsPath = resolve(__dirname, '../build-wasm/kicad_drc.mjs');

console.log('Loading WASM module with assertion interception...');

try {
    const { default: createKicadDRC } = await import(mjsPath);

    const Module = await createKicadDRC({
        print: (text) => console.log('[stdout]', text),
        printErr: (text) => console.error('[stderr]', text),
    });

    console.log('WASM module loaded successfully!');
    console.log('Exported kicad_ functions:',
        Object.keys(Module).filter(k => k.startsWith('_kicad')));
} catch (e) {
    console.error('=== CRASH ===');
    console.error('Type:', e.constructor.name);
    console.error('Message:', e.message);

    // Try to decode the stack
    if (e.stack) {
        const lines = e.stack.split('\n');
        for (const line of lines) {
            console.error('  ', line.trim());
        }
    }
}
