/**
 * Trace test: try to figure out which static constructor is crashing.
 * Uses Node's V8 WASM debugging features.
 */
import { readFileSync } from 'fs';
import { dirname, resolve } from 'path';
import { fileURLToPath } from 'url';

const __dirname = dirname(fileURLToPath(import.meta.url));
const wasmPath = resolve(__dirname, '../build-wasm/kicad_drc.wasm');
const mjsPath = resolve(__dirname, '../build-wasm/kicad_drc.mjs');

// Read and parse the symbol map
const symbolMapPath = resolve(__dirname, '../build-wasm/kicad_drc.mjs.symbols');
const symbolMap = new Map();
try {
    const lines = readFileSync(symbolMapPath, 'utf-8').split('\n');
    for (const line of lines) {
        const [idx, ...nameParts] = line.split(':');
        if (idx && nameParts.length) {
            symbolMap.set(parseInt(idx), nameParts.join(':'));
        }
    }
    console.log(`Loaded ${symbolMap.size} symbols`);
} catch (e) {
    console.log('No symbol map found');
}

console.log('Loading WASM module...');

try {
    const { default: createKicadDRC } = await import(mjsPath);

    const Module = await createKicadDRC({
        print: (text) => console.log('[stdout]', text),
        printErr: (text) => console.error('[stderr]', text),
        onAbort: (what) => {
            console.error('=== JS ABORT ===');
            console.error('Reason:', what);
            console.error('Stack:', new Error().stack);
        },
    });

    console.log('WASM module loaded successfully!');
    console.log('diag_ping:', Module._diag_ping());
} catch (e) {
    console.error('=== CRASH ===');
    console.error('Type:', e.constructor.name);
    console.error('Message:', e.message);

    // Parse the wasm-function index from the stack trace
    const funcMatch = e.stack?.match(/wasm-function\[(\d+)\]/);
    if (funcMatch) {
        const funcIdx = parseInt(funcMatch[1]);
        const funcName = symbolMap.get(funcIdx);
        console.error(`Crashing function index: ${funcIdx}`);
        console.error(`Function name: ${funcName || 'unknown (not in symbol map)'}`);
    }

    // Show offset for looking up in disassembly
    const offsetMatch = e.stack?.match(/:0x([0-9a-f]+)/);
    if (offsetMatch) {
        console.error(`Crash offset: 0x${offsetMatch[1]}`);
    }

    if (e.stack) {
        console.error('\nFull stack:', e.stack);
    }
}
