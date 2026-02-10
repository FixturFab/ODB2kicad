// Integration test for the odb2kicad-wasm JS wrapper
// Tests both convertOdb() and convertOdbArchive() APIs.

import { readFileSync, readdirSync, statSync, existsSync } from 'fs';
import { join, relative, dirname } from 'path';
import { fileURLToPath } from 'url';
import { strict as assert } from 'assert';
import { execSync } from 'child_process';

const __dirname = dirname(fileURLToPath(import.meta.url));

// Import from dist/ (the built package)
const { convertOdb, convertOdbArchive } = await import(join(__dirname, '..', 'dist', 'index.mjs'));

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

// === Archive extraction tests ===

// Create test archives (tgz and zip) from sample directories
const tmpDir = '/tmp';
execSync(`tar czf ${tmpDir}/odb-output.tgz -C "${samplesDir}" odb-output`);
execSync(`tar czf ${tmpDir}/odb-kitchen-sink.tgz -C "${samplesDir}" odb-kitchen-sink`);
execSync(`cd "${samplesDir}" && zip -rq ${tmpDir}/odb-output.zip odb-output`);
execSync(`cd "${samplesDir}" && zip -rq ${tmpDir}/odb-kitchen-sink.zip odb-kitchen-sink`);

// Get reference output from directory-based conversion
const refSimple = await convertOdb(loadDir(join(samplesDir, 'odb-output')));
const refKitchen = await convertOdb(loadDir(join(samplesDir, 'odb-kitchen-sink')));

// --- Test 6: .tgz archive (simple) ---
console.log('--- Test 6: .tgz archive (odb-output) ---');
{
  const buf = readFileSync(join(tmpDir, 'odb-output.tgz'));
  const result = await convertOdbArchive(buf);

  assert.equal(result.success, true, 'tgz conversion should succeed');
  assert.ok(result.kicadPcb.includes('kicad_pcb'), 'output should contain kicad_pcb');
  assert.equal(result.kicadPcb, refSimple.kicadPcb, 'tgz output should match directory-based output');
  console.log(`  PASS: ${result.kicadPcb.length} chars, identical to directory conversion`);
}

// --- Test 7: .zip archive (simple) ---
console.log('--- Test 7: .zip archive (odb-output) ---');
{
  const buf = readFileSync(join(tmpDir, 'odb-output.zip'));
  const result = await convertOdbArchive(buf);

  assert.equal(result.success, true, 'zip conversion should succeed');
  assert.ok(result.kicadPcb.includes('kicad_pcb'), 'output should contain kicad_pcb');
  assert.equal(result.kicadPcb, refSimple.kicadPcb, 'zip output should match directory-based output');
  console.log(`  PASS: ${result.kicadPcb.length} chars, identical to directory conversion`);
}

// --- Test 8: .tgz archive (kitchen-sink) ---
console.log('--- Test 8: .tgz archive (odb-kitchen-sink) ---');
{
  const buf = readFileSync(join(tmpDir, 'odb-kitchen-sink.tgz'));
  const result = await convertOdbArchive(buf);

  assert.equal(result.success, true, 'tgz kitchen-sink should succeed');
  assert.equal(result.kicadPcb, refKitchen.kicadPcb, 'tgz kitchen-sink should match directory-based output');
  console.log(`  PASS: ${result.kicadPcb.length} chars, identical to directory conversion`);
}

// --- Test 9: .zip archive (kitchen-sink) ---
console.log('--- Test 9: .zip archive (odb-kitchen-sink) ---');
{
  const buf = readFileSync(join(tmpDir, 'odb-kitchen-sink.zip'));
  const result = await convertOdbArchive(buf);

  assert.equal(result.success, true, 'zip kitchen-sink should succeed');
  assert.equal(result.kicadPcb, refKitchen.kicadPcb, 'zip kitchen-sink should match directory-based output');
  console.log(`  PASS: ${result.kicadPcb.length} chars, identical to directory conversion`);
}

// --- Test 10: Invalid archive ---
console.log('--- Test 10: Invalid archive ---');
{
  const buf = new Uint8Array([0xDE, 0xAD, 0xBE, 0xEF]);
  const result = await convertOdbArchive(buf);

  assert.equal(result.success, false, 'invalid archive should fail');
  assert.ok(result.error, 'error message should be present');
  console.log(`  PASS: Error for invalid archive: "${result.error}"`);
}

// --- Test 11: Empty ArrayBuffer ---
console.log('--- Test 11: Empty ArrayBuffer ---');
{
  const result = await convertOdbArchive(new ArrayBuffer(0));

  assert.equal(result.success, false, 'empty buffer should fail');
  assert.ok(result.error, 'error message should be present');
  console.log(`  PASS: Error for empty buffer: "${result.error}"`);
}

console.log('\nAll tests PASSED');
