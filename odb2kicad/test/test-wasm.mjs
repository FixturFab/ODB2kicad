// Stage 3: End-to-end WASM test — load ODB++ into MEMFS and convert
import { readFileSync, readdirSync, statSync } from 'fs';
import { join, relative } from 'path';
import { createRequire } from 'module';

const require = createRequire(import.meta.url);

// Recursively load all files from a directory into an array of {path, content}
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

// Recursively create directories in MEMFS
function mkdirp(FS, path) {
    const parts = path.split('/').filter(Boolean);
    let current = '';
    for (const part of parts) {
        current += '/' + part;
        try {
            FS.mkdir(current);
        } catch (e) {
            // already exists
        }
    }
}

async function main() {
    const odbDir = process.argv[2] || join(import.meta.url.replace('file://', ''), '../../..', 'samples/odb-output');
    const resolvedDir = odbDir.startsWith('/') ? odbDir : join(process.cwd(), odbDir);

    console.log(`Loading ODB++ from: ${resolvedDir}`);

    // Load all files from the ODB++ directory
    const files = loadDir(resolvedDir);
    console.log(`Loaded ${files.length} files`);

    // Load WASM module
    const wasmPath = join(import.meta.url.replace('file://', ''), '../../build-wasm/odb2kicad_wasm.mjs');
    const initModule = (await import(wasmPath)).default;
    const Module = await initModule();

    console.log('WASM module loaded');
    console.log(`convertOdb available: ${typeof Module.convertOdb === 'function'}`);

    // Write files into MEMFS under /odb
    const FS = Module.FS;
    mkdirp(FS, '/odb');

    for (const file of files) {
        const fullPath = '/odb/' + file.path;
        const dir = fullPath.substring(0, fullPath.lastIndexOf('/'));
        mkdirp(FS, dir);
        FS.writeFile(fullPath, file.content);
    }

    console.log(`Wrote ${files.length} files to MEMFS /odb/`);

    // Run conversion
    console.log('Running convertOdb("/odb")...');
    const result = Module.convertOdb('/odb');

    if (result.startsWith('ERROR:')) {
        console.error('Conversion FAILED:', result);
        process.exit(1);
    }

    console.log(`\nConversion SUCCESS — output is ${result.length} characters`);

    // Validate output
    let errors = 0;

    if (!result.includes('kicad_pcb')) {
        console.error('FAIL: output does not contain "kicad_pcb"');
        errors++;
    } else {
        console.log('PASS: output contains "kicad_pcb"');
    }

    if (!result.includes('(net ')) {
        console.error('FAIL: output does not contain "(net "');
        errors++;
    } else {
        console.log('PASS: output contains net definitions');
    }

    if (!result.includes('(module ') && !result.includes('(footprint ')) {
        console.error('FAIL: output does not contain footprints');
        errors++;
    } else {
        console.log('PASS: output contains footprints');
    }

    if (!result.includes('(segment ') && !result.includes('(gr_line ')) {
        console.error('FAIL: output does not contain traces/graphics');
        errors++;
    } else {
        console.log('PASS: output contains traces/graphics');
    }

    // Print first/last few lines for inspection
    const lines = result.split('\n');
    console.log(`\nOutput: ${lines.length} lines`);
    console.log('--- First 5 lines ---');
    for (let i = 0; i < Math.min(5, lines.length); i++) {
        console.log(lines[i]);
    }
    console.log('--- Last 5 lines ---');
    for (let i = Math.max(0, lines.length - 5); i < lines.length; i++) {
        console.log(lines[i]);
    }

    if (errors > 0) {
        console.error(`\n${errors} check(s) FAILED`);
        process.exit(1);
    }

    console.log('\nAll checks PASSED');
}

main().catch(e => {
    console.error('Fatal error:', e);
    process.exit(1);
});
