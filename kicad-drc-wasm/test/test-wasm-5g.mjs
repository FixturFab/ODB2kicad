/**
 * Stage 5g: Full end-to-end WASM DRC test.
 *
 * Usage: node test-wasm-5g.mjs [path-to-kicad_pcb]
 * Default: uses /root/kicad-test/samples/test.kicad_pcb
 */
import { dirname, resolve } from 'path';
import { fileURLToPath } from 'url';
import { readFileSync } from 'fs';

const __dirname = dirname(fileURLToPath(import.meta.url));
const pcbPath = process.argv[2] || '/root/kicad-test/samples/test.kicad_pcb';

console.log('=== Stage 5g: WASM DRC Test ===');
console.log('PCB file:', pcbPath);
console.log();

// Step 1: Load the WASM module
console.log('[1/5] Loading WASM module...');
const mjsPath = resolve(__dirname, '../build-wasm/kicad_drc.mjs');
const { default: createKicadDRC } = await import(mjsPath);

const Module = await createKicadDRC({
    print: (text) => console.log('[wasm:out]', text),
    printErr: (text) => console.error('[wasm:err]', text),
    locateFile: (path) => resolve(__dirname, '../build-wasm/', path),
});
console.log('[1/5] Module loaded successfully.');
console.log();

// Step 2: Verify diag_ping
console.log('[2/5] Testing diag_ping...');
const pingResult = Module._diag_ping();
console.log('[2/5] diag_ping returned:', pingResult);
if (pingResult !== 42) {
    console.error('FAIL: Expected 42, got', pingResult);
    process.exit(1);
}
console.log();

// Step 3: Load PCB file
console.log('[3/5] Loading PCB file...');
const pcbContent = readFileSync(pcbPath, 'utf8');
console.log('  PCB file size:', pcbContent.length, 'bytes');

// Allocate WASM memory and copy the PCB content
const len = Module.lengthBytesUTF8(pcbContent) + 1;
const ptr = Module._malloc(len);
Module.stringToUTF8(pcbContent, ptr, len);

const loadResult = Module._kicad_load_pcb(ptr, 0);
Module._free(ptr);

console.log('[3/5] kicad_load_pcb returned:', loadResult);
if (loadResult !== 0) {
    console.error('FAIL: PCB load failed with code', loadResult);
    process.exit(1);
}
console.log();

// Step 4: Run DRC
console.log('[4/5] Running DRC...');
const violationCount = Module._kicad_run_drc();
console.log('[4/5] kicad_run_drc returned:', violationCount, 'violations');
if (violationCount < 0) {
    console.error('FAIL: DRC failed with code', violationCount);
    process.exit(1);
}
console.log();

// Step 5: Get JSON results
console.log('[5/5] Getting DRC results...');
const jsonPtr = Module._kicad_get_drc_results();
if (jsonPtr === 0) {
    console.error('FAIL: kicad_get_drc_results returned null');
    process.exit(1);
}
const jsonStr = Module.UTF8ToString(jsonPtr);
console.log('  JSON result length:', jsonStr.length, 'bytes');

// Parse and display
const results = JSON.parse(jsonStr);
console.log('  Schema:', results.$schema || '(none)');
console.log('  Coordinate units:', results.coordinate_units);
console.log('  Violations:', results.violations?.length ?? 0);
console.log('  Unconnected items:', results.unconnected_items?.length ?? 0);

if (results.violations && results.violations.length > 0) {
    console.log('\n  Violation details:');
    for (const v of results.violations) {
        console.log(`    - [${v.severity}] ${v.type}: ${v.description}`);
    }
}

// Cleanup
Module._kicad_cleanup();

console.log('\n=== RESULT: PASS ===');
console.log(`DRC completed with ${violationCount} violations in WASM`);
