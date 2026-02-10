// Stage 3: Parity test — compare WASM output to native CLI output
import { readFileSync, writeFileSync, readdirSync, statSync } from 'fs';
import { join, relative } from 'path';

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

function mkdirp(FS, path) {
    const parts = path.split('/').filter(Boolean);
    let current = '';
    for (const part of parts) {
        current += '/' + part;
        try { FS.mkdir(current); } catch (e) {}
    }
}

async function main() {
    const odbDir = process.argv[2];
    const outputFile = process.argv[3];
    if (!odbDir || !outputFile) {
        console.error('Usage: node test-wasm-parity.mjs <odb-dir> <output-file>');
        process.exit(1);
    }

    const files = loadDir(odbDir);
    const wasmPath = join(import.meta.url.replace('file://', ''), '../../build-wasm/odb2kicad_wasm.mjs');
    const initModule = (await import(wasmPath)).default;
    const Module = await initModule();
    const FS = Module.FS;

    mkdirp(FS, '/odb');
    for (const file of files) {
        const fullPath = '/odb/' + file.path;
        const dir = fullPath.substring(0, fullPath.lastIndexOf('/'));
        mkdirp(FS, dir);
        FS.writeFile(fullPath, file.content);
    }

    const result = Module.convertOdb('/odb');
    if (result.startsWith('ERROR:')) {
        console.error('Conversion failed:', result);
        process.exit(1);
    }

    writeFileSync(outputFile, result);
    console.log(`Wrote ${result.length} bytes to ${outputFile}`);
}

main().catch(e => { console.error(e); process.exit(1); });
