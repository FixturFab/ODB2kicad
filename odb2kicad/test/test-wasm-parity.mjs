// Stage 3: Parity test — compare WASM output to native CLI output
import { readFileSync, writeFileSync, readdirSync, statSync } from 'fs';
import { dirname, join, relative, sep } from 'path';
import { fileURLToPath, pathToFileURL } from 'url';

const __dirname = dirname(fileURLToPath(import.meta.url));

function loadDir(dir, base = dir) {
    let files = [];
    for (const entry of readdirSync(dir)) {
        const full = join(dir, entry);
        // MEMFS paths always use '/', whatever the host separator is
        const rel = relative(base, full).split(sep).join('/');
        const st = statSync(full);
        if (st.isDirectory()) {
            files = files.concat(loadDir(full, base));
        } else {
            // bytes, not text: compressed <name>.z files are binary, and text files go in unchanged
            files.push({ path: rel, content: new Uint8Array(readFileSync(full)) });
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
    // import() needs a file:// URL; a bare path such as C:\... is rejected on Windows
    const wasmPath = join(__dirname, '..', 'build-wasm', 'odb2kicad_wasm.mjs');
    const initModule = (await import(pathToFileURL(wasmPath).href)).default;
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
