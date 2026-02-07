/**
 * Stage 5g: Test WASM DRC module in Node.js
 *
 * Usage: node test/test-wasm.mjs <path-to-kicad_pcb>
 */
import { readFileSync } from 'fs';
import { resolve } from 'path';
import createKicadDRC from '../build-wasm/kicad_drc.mjs';

const pcbPath = process.argv[2];
if (!pcbPath) {
    console.error('Usage: node test/test-wasm.mjs <path-to-kicad_pcb>');
    process.exit(1);
}

console.log(`Loading WASM module...`);
const Module = await createKicadDRC();
console.log(`WASM module loaded.`);

// Read the PCB file
const pcbContent = readFileSync(resolve(pcbPath), 'utf-8');
console.log(`Read PCB file: ${pcbPath} (${pcbContent.length} bytes)`);

// Allocate memory in WASM heap and copy the PCB content
const len = Module.lengthBytesUTF8(pcbContent) + 1;
const ptr = Module._malloc(len);
Module.stringToUTF8(pcbContent, ptr, len);

// Load the PCB
console.log(`Loading PCB into WASM...`);
const loadResult = Module._kicad_load_pcb(ptr, 0);
Module._free(ptr);

if (loadResult !== 0) {
    console.error(`kicad_load_pcb failed with code: ${loadResult}`);
    process.exit(2);
}
console.log(`PCB loaded successfully.`);

// Run DRC
console.log(`Running DRC...`);
const violationCount = Module._kicad_run_drc();
console.log(`DRC complete. Violations: ${violationCount}`);

// Get JSON results
const jsonPtr = Module._kicad_get_drc_results();
if (jsonPtr) {
    const jsonStr = Module.UTF8ToString(jsonPtr);
    try {
        const result = JSON.parse(jsonStr);
        console.log(`\nDRC Report:`);
        console.log(`  Source: ${result.source}`);
        console.log(`  Coordinate units: ${result.coordinate_units}`);
        console.log(`  Violations: ${(result.violations || []).length}`);
        console.log(`  Unconnected items: ${(result.unconnected_items || []).length}`);
        console.log(`  Schematic parity: ${(result.schematic_parity || []).length}`);

        if (result.violations && result.violations.length > 0) {
            console.log(`\nViolation details:`);
            for (const v of result.violations) {
                console.log(`  - [${v.severity}] ${v.type}: ${v.description}`);
            }
        }

        // Output full JSON for comparison
        console.log(`\n--- Full JSON output ---`);
        console.log(JSON.stringify(result, null, 2));
    } catch (e) {
        console.error(`Failed to parse JSON result: ${e.message}`);
        console.log(`Raw JSON (first 500 chars): ${jsonStr.substring(0, 500)}`);
    }
} else {
    console.log(`No JSON results returned (null pointer).`);
}

// Cleanup
Module._kicad_cleanup();
console.log(`\nCleanup done.`);
