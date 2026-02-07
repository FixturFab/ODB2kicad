/**
 * Debug: try to load the WASM module and decode the crash location
 */
import { dirname, resolve } from 'path';
import { fileURLToPath } from 'url';
import { readFileSync } from 'fs';

const __dirname = dirname(fileURLToPath(import.meta.url));

// Read symbol map
const symbolMap = {};
const lines = readFileSync(resolve(__dirname, '../build-wasm/kicad_drc.mjs.symbols'), 'utf8').split('\n');
for (const line of lines) {
    const colon = line.indexOf(':');
    if (colon !== -1) {
        symbolMap[line.substring(0, colon)] = line.substring(colon + 1);
    }
}

function decodeStack(stack) {
    if (!stack) return;
    const funcMatches = stack.matchAll(/wasm-function\[(\d+)\]/g);
    if (funcMatches) {
        console.error('\nWASM call stack (decoded):');
        for (const m of funcMatches) {
            const idx = m[1];
            const name = symbolMap[idx] || '(unknown)';
            console.error(`  Function[${idx}] = ${name}`);
        }
    }
}

const mjsPath = resolve(__dirname, '../build-wasm/kicad_drc.mjs');
const { default: createKicadDRC } = await import(mjsPath);

try {
    const Module = await createKicadDRC({
        print: (text) => console.log('[out]', text),
        printErr: (text) => {
            console.error('[err]', text);
            // Check if this is a stack trace
            if (text.includes('wasm-function')) {
                decodeStack(text);
            }
        },
        onAbort: (what) => {
            console.error('=== ABORT called ===');
            console.error('Reason:', what);
        },
        locateFile: (path) => resolve(__dirname, '../build-wasm/', path),
    });
    console.log('Module loaded successfully!');
    console.log('Available exports:', Object.keys(Module).filter(k => k.startsWith('_kicad') || k.startsWith('_diag')));

    // Try calling diag_ping
    const result = Module._diag_ping();
    console.log('diag_ping returned:', result);
} catch (e) {
    console.error('\n=== EXCEPTION ===');
    console.error('Type:', e.constructor.name);
    console.error('Message:', e.message);
    console.error('Stack:', e.stack);
    decodeStack(e.stack);
}
