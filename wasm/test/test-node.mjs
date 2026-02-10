// Stage 4: Integration test for the odb2kicad-wasm JS wrapper
// Tests the high-level convertOdb() API against sample ODB++ directories.

import { readFileSync, readdirSync, statSync } from 'fs';
import { join, relative, dirname } from 'path';
import { fileURLToPath } from 'url';
import { strict as assert } from 'assert';

const __dirname = dirname(fileURLToPath(import.meta.url));

// Import from dist/ (the built package)
const { convertOdb } = await import(join(__dirname, '..', 'dist', 'index.mjs'));

// Recursively load all files from a directory
function loadDir(dir, base = dir) {
  let files = [];
  for (const entry of readdirSync(dir)) {
    const full = join(dir, entry);
    const rel = relative(base, full);
    const st = statSync(full);
    if (st.isDirectory()) {
      files = files.concat(loadDir(full, base));
    } else {
      files.push({ path: rel, content: readFileSync(full, 'utf-8') });
    }
  }
  return files;
}

const samplesDir = join(__dirname, '..', '..', 'samples');

// --- Test 1: Simple ODB++ sample (odb-output) ---
console.log('--- Test 1: odb-output (simple) ---');
{
  const files = loadDir(join(samplesDir, 'odb-output'));
  console.log(`  Loaded ${files.length} files`);

  const result = await convertOdb(files);

  assert.equal(result.success, true, 'conversion should succeed');
  assert.ok(result.kicadPcb, 'kicadPcb should be present');
  assert.ok(result.kicadPcb.includes('kicad_pcb'), 'output should contain kicad_pcb');
  assert.ok(result.kicadPcb.includes('(net '), 'output should contain net definitions');
  assert.ok(
    result.kicadPcb.includes('(module ') || result.kicadPcb.includes('(footprint '),
    'output should contain footprints'
  );

  console.log(`  PASS: ${result.kicadPcb.length} chars, ${result.kicadPcb.split('\n').length} lines`);
}

// --- Test 2: Kitchen-sink ODB++ sample ---
console.log('--- Test 2: odb-kitchen-sink ---');
{
  const files = loadDir(join(samplesDir, 'odb-kitchen-sink'));
  console.log(`  Loaded ${files.length} files`);

  const result = await convertOdb(files);

  assert.equal(result.success, true, 'conversion should succeed');
  assert.ok(result.kicadPcb.includes('kicad_pcb'), 'output should contain kicad_pcb');
  assert.ok(result.kicadPcb.includes('(net '), 'output should contain net definitions');
  assert.ok(
    result.kicadPcb.includes('(segment ') || result.kicadPcb.includes('(gr_line '),
    'output should contain traces/graphics'
  );

  console.log(`  PASS: ${result.kicadPcb.length} chars, ${result.kicadPcb.split('\n').length} lines`);
}

// --- Test 3: Verify WASM output matches native CLI ---
console.log('--- Test 3: WASM vs native parity ---');
{
  const files = loadDir(join(samplesDir, 'odb-output'));
  const result = await convertOdb(files);

  // Run native CLI for comparison (no output arg = writes to stdout)
  const { execSync } = await import('child_process');
  const nativeBin = join(__dirname, '..', '..', 'odb2kicad', 'build', 'odb2kicad');
  const nativeOutput = execSync(
    `"${nativeBin}" "${join(samplesDir, 'odb-output')}"`,
    { encoding: 'utf-8' }
  );

  assert.equal(result.kicadPcb, nativeOutput, 'WASM output should be byte-identical to native CLI');
  console.log('  PASS: WASM output is byte-identical to native CLI');
}

// --- Test 4: Error handling — empty directory ---
console.log('--- Test 4: Error handling ---');
{
  const result = await convertOdb([]);
  assert.equal(result.success, false, 'empty input should fail');
  assert.ok(result.error, 'error message should be present');
  console.log(`  PASS: Error for empty input: "${result.error}"`);
}

// --- Test 5: Multiple conversions (tests MEMFS cleanup) ---
console.log('--- Test 5: Multiple sequential conversions ---');
{
  const files = loadDir(join(samplesDir, 'odb-output'));

  const r1 = await convertOdb(files);
  const r2 = await convertOdb(files);

  assert.equal(r1.success, true);
  assert.equal(r2.success, true);
  assert.equal(r1.kicadPcb, r2.kicadPcb, 'repeated conversions should produce identical output');
  console.log('  PASS: Two sequential conversions produce identical output');
}

console.log('\nAll tests PASSED');
