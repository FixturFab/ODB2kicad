// Integration test for the odb2kicad-wasm JS wrapper
// Tests both convertOdb() and convertOdbArchive() APIs.

import { readFileSync, writeFileSync, readdirSync, statSync, existsSync, mkdtempSync } from 'fs';
import { join, relative, dirname } from 'path';
import { tmpdir } from 'os';
import { fileURLToPath } from 'url';
import { strict as assert } from 'assert';
import { execSync } from 'child_process';
import { zipSync } from 'fflate';

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
      // compressed ODB++ files (<name>.z, UNIX compress) are bytes, everything else text
      files.push({ path: rel, content: /\.z$/i.test(rel) ? new Uint8Array(readFileSync(full)) : readFileSync(full, 'utf-8') });
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

// Zip a sample directory with fflate (already a dependency), so the test does not
// need the zip CLI. Entries are prefixed with the directory name, as `zip -r` writes them.
function zipDir(name) {
  const tree = {};
  for (const f of loadDir(join(samplesDir, name))) {
    tree[`${name}/${f.path}`] = new Uint8Array(readFileSync(join(samplesDir, name, f.path)));
  }
  return zipSync(tree);
}

// Create test archives (tgz and zip) from sample directories
const tmpDir = mkdtempSync(join(tmpdir(), 'odb2kicad-wasm-test-'));
execSync(`tar czf "${tmpDir}/odb-output.tgz" -C "${samplesDir}" odb-output`);
execSync(`tar czf "${tmpDir}/odb-kitchen-sink.tgz" -C "${samplesDir}" odb-kitchen-sink`);
writeFileSync(join(tmpDir, 'odb-output.zip'), zipDir('odb-output'));
writeFileSync(join(tmpDir, 'odb-kitchen-sink.zip'), zipDir('odb-kitchen-sink'));

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

// --- Test 12: DNP footprints (odb-dnp: R1 has .no_pop, R2 a 'Config' field 'Rev B; DNP') ---
console.log('--- Test 12: DNP footprints (odb-dnp) ---');
{
  const result = await convertOdb(loadDir(join(samplesDir, 'odb-dnp')));

  assert.equal(result.success, true, 'conversion should succeed');
  const dnp = result.kicadPcb.split('\n').filter((l) => l.includes('(attr smd dnp)')).length;
  assert.equal(dnp, 2, 'both the .no_pop footprint and the Config DNP footprint should be dnp');
  assert.ok(!refSimple.kicadPcb.includes(' dnp)'), 'a board without DNP footprints should have no dnp attr');
  console.log(`  PASS: ${dnp} dnp footprints, none on odb-output`);
}

// --- Test 13: failure reasons: a code and a reason for each kind of bad input ---
console.log('--- Test 13: Failure reasons ---');
{
  const matrix = 'STEP {\n NAME=PCB\n}\nLAYER {\n ROW=1\n NAME=TOP\n TYPE=SIGNAL\n}\n';
  const cases = [
    ['not an array', () => convertOdb('matrix/matrix'), 'bad-input'],
    ['no files', () => convertOdb([]), 'not-odb'],
    ['no matrix', () => convertOdb([{ path: 'gerbers/top.gbr', content: 'G04*' }]), 'not-odb'],
    ['empty matrix', () => convertOdb([{ path: 'matrix/matrix', content: 'STEP {\n NAME=PCB\n}\n' }]), 'bad-matrix'],
    ['no step', () => convertOdb([{ path: 'matrix/matrix', content: matrix }]), 'no-step'],
    ['no layers', () => convertOdb([{ path: 'matrix/matrix', content: matrix }, { path: 'steps/pcb/profile', content: '' }]), 'no-layers'],
    ['bad archive', () => convertOdbArchive(new Uint8Array([0xDE, 0xAD, 0xBE, 0xEF])), 'archive'],
    ['not a buffer', () => convertOdbArchive('x'), 'bad-input'],
  ];
  for (const [name, run, code] of cases) {
    const r = await run();
    assert.equal(r.success, false, `${name} should fail`);
    assert.equal(r.code, code, `${name}: code ${r.code} (${r.error})`);
    assert.ok(r.error && r.error.length > 10, `${name}: a reason`);
    console.log(`  PASS: ${name}: ${r.code}: ${r.error}`);
  }
  // the module still converts after the failures
  const ok = await convertOdb(loadDir(join(samplesDir, 'odb-output')));
  assert.equal(ok.success, true, 'a good board still converts after failures');
}

// --- Test 14: a job with two steps ---
console.log('--- Test 14: Multi-step job ---');
{
  const layerRow = (row, type, name) => `LAYER {\n ROW=${row}\n CONTEXT=BOARD\n TYPE=${type}\n NAME=${name}\n POLARITY=POSITIVE\n}\n`;
  const step = (name, x) => [
    { path: `steps/${name}/stephdr`, content: 'UNITS=MM\n' },
    { path: `steps/${name}/profile`, content: 'UNITS=MM\nS P 0\nOB 0 0 I\nOS 0 20\nOS 40 20\nOS 40 0\nOS 0 0\nOE\nSE\n' },
    { path: `steps/${name}/layers/top/features`, content: `UNITS=MM\n$0 r1000\nP ${x} 5 0 P 0 0\n` },
    { path: `steps/${name}/layers/bot/features`, content: `UNITS=MM\n$0 r1000\nP ${x} 5 0 P 0 0\n` },
  ];
  const files = [
    { path: 'misc/info', content: 'JOB_NAME=two\nUNITS=MM\n' },
    { path: 'matrix/matrix', content: 'STEP {\n COL=2\n NAME=FLEX\n}\nSTEP {\n COL=1\n NAME=PCB\n}\n' + layerRow(1, 'SIGNAL', 'TOP') + layerRow(2, 'SIGNAL', 'BOT') },
    ...step('pcb', 5), ...step('flex', 7),
  ];
  const first = await convertOdb(files);
  assert.equal(first.success, true, first.error);
  assert.deepEqual(first.steps, ['PCB', 'FLEX'], 'steps in COL order');
  assert.equal(first.step, 'PCB', 'the first step by COL is converted by default');
  assert.ok(first.report.some((l) => l.startsWith('info: the job has 2 steps')), 'the report names the steps');
  const flex = await convertOdb(files, { step: 'flex' });
  assert.equal(flex.success, true, flex.error);
  assert.equal(flex.step, 'FLEX');
  assert.notEqual(flex.kicadPcb, first.kicadPcb, 'the other step is another board');
  const none = await convertOdb(files, { step: 'nope' });
  assert.equal(none.success, false);
  assert.equal(none.code, 'no-step');
  const one = await convertOdb(loadDir(join(samplesDir, 'odb-output')));
  assert.equal(one.steps.length, 1, 'a one-step job lists its one step');
  assert.ok(one.report.some((l) => l.startsWith('info: ODB++ version 8.1')), 'the report gives the ODB++ version');
  assert.ok(one.report.some((l) => /^info: matrix row \d+ F\.CU \(SIGNAL\) -> F\.Cu$/.test(l)), 'the report lists the matrix rows');
  assert.ok(/\(generator "odb2kicad\/[\d.]+ odb\+\+:v8\.1"\)/.test(one.kicadPcb), 'the generator names the ODB++ version');
  assert.equal(one.steps[0].toLowerCase(), 'pcb');
  console.log(`  PASS: steps ${first.steps.join(', ')}; default ${first.step}; { step: 'flex' } converts ${flex.step}; unknown step: ${none.code}`);
}

// --- Test 15: demo-design (compressed files, text, drill tools) matches the native CLI ---
console.log('--- Test 15: demo-design (compressed .z files) vs native ---');
{
  const dir = join(samplesDir, 'demo-design');
  const files = loadDir(dir);
  const z = files.filter((f) => f.content instanceof Uint8Array).length;
  assert.ok(z > 10, 'the fixture holds compressed files');
  const result = await convertOdb(files);
  assert.equal(result.success, true, result.error);
  const nativeBin = join(__dirname, '..', '..', 'odb2kicad', 'build', 'odb2kicad');
  const nativeOutput = execSync(`"${nativeBin}" "${dir}"`, { encoding: 'utf-8', maxBuffer: 256 * 1024 * 1024 });
  assert.equal(result.kicadPcb === nativeOutput, true, 'WASM output should be byte-identical to native CLI on demo-design');
  assert.ok(/\(footprint "[^"]*" \(layer "B\.Cu"\)/.test(result.kicadPcb), 'bottom components (components.z) are read');
  // the same job as a .tgz: the archive path keeps the compressed files as bytes too
  execSync(`tar czf "${tmpDir}/demo-design.tgz" -C "${samplesDir}" demo-design`);
  const fromTgz = await convertOdbArchive(new Uint8Array(readFileSync(join(tmpDir, 'demo-design.tgz'))));
  assert.equal(fromTgz.success, true, fromTgz.error);
  assert.equal(fromTgz.kicadPcb === result.kicadPcb, true, 'the archive converts to the same board');
  console.log(`  PASS: ${files.length} files (${z} compressed), ${result.kicadPcb.length} chars, identical to native and to its .tgz`);
}

// --- Test 16: a panel step (STEP-REPEAT) places its step, the same as the native CLI ---
console.log('--- Test 16: Panel step (STEP-REPEAT) vs native ---');
{
  const layerRow = (row, type, name) => `LAYER {\n ROW=${row}\n CONTEXT=BOARD\n TYPE=${type}\n NAME=${name}\n POLARITY=POSITIVE\n}\n`;
  const files = [
    { path: 'misc/info', content: 'JOB_NAME=panel\nUNITS=MM\n' },
    { path: 'matrix/matrix', content: 'STEP {\n COL=1\n NAME=PCB\n}\nSTEP {\n COL=2\n NAME=PANEL\n}\n' + layerRow(1, 'COMPONENT', 'COMP_+_TOP') + layerRow(2, 'SIGNAL', 'TOP') },
    { path: 'steps/pcb/stephdr', content: 'UNITS=MM\nX_DATUM=3\nY_DATUM=4\n' },
    { path: 'steps/pcb/profile', content: 'UNITS=MM\nS P 0\nOB 0 0 I\nOS 0 20\nOS 40 20\nOS 40 0\nOS 0 0\nOE\nSE\n' },
    { path: 'steps/pcb/layers/top/features', content: 'UNITS=MM\n$0 r1000\nP 10 5 0 P 0 0\nL 10 5 20 5 0 P 0\n' },
    { path: 'steps/pcb/layers/comp_+_top/components', content: 'UNITS=MM\nCMP 0 10 5 0 N R1 pkg\nTOP 0 10 5 0 N 0 0 1\n' },
    { path: 'steps/pcb/netlists/cadnet/netlist', content: 'UNITS=MM\n$0 N1\n0 0.5 10 5 T 1 1 e e s\n' },
    { path: 'steps/panel/stephdr', content: 'UNITS=MM\nSTEP-REPEAT {\n NAME=PCB\n X=100\n Y=50\n DX=40\n DY=30\n NX=2\n NY=2\n ANGLE=90\n MIRROR=NO\n}\n' },
  ];
  const panel = await convertOdb(files, { step: 'panel' });
  assert.equal(panel.success, true, panel.error);
  assert.equal(panel.step, 'PANEL');
  assert.equal((panel.kicadPcb.match(/\(footprint /g) || []).length, 4, 'four copies of R1');
  assert.ok(panel.kicadPcb.includes('(at 101 -43 -90)'), 'the first copy turned 90 deg about the datum');
  assert.ok(panel.kicadPcb.includes('"Board_4-N1"'), 'each copy has its own nets');
  assert.ok(panel.report.some((l) => l.startsWith('info: panel PANEL: step PCB placed 2 x 2')), 'the report says what was placed');
  const dir = mkdtempSync(join(tmpdir(), 'odb2kicad-panel-'));
  for (const f of files) {
    execSync(`mkdir -p "${dirname(join(dir, f.path))}"`);
    writeFileSync(join(dir, f.path), f.content);
  }
  const nativeBin = join(__dirname, '..', '..', 'odb2kicad', 'build', 'odb2kicad');
  const nativeOutput = execSync(`"${nativeBin}" --step panel "${dir}"`, { encoding: 'utf-8' });
  assert.equal(panel.kicadPcb === nativeOutput, true, 'WASM output should be byte-identical to native CLI on a panel');
  console.log('  PASS: 2 x 2 copies at 90 deg, nets Board_<n>-, identical to native');
}

console.log('\nAll tests PASSED');
