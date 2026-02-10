// odb2kicad-wasm — High-level JavaScript API
// Wraps the Emscripten WASM module with a clean convertOdb() interface.

import { createRequire } from 'module';
import { dirname, join } from 'path';
import { fileURLToPath } from 'url';

const __dirname = dirname(fileURLToPath(import.meta.url));

let _modulePromise = null;

/**
 * Lazily initialize and cache the WASM module.
 * @returns {Promise<object>} The initialized Emscripten module.
 */
function getModule() {
  if (!_modulePromise) {
    _modulePromise = (async () => {
      const initModule = (await import(join(__dirname, 'odb2kicad_wasm.mjs'))).default;
      return await initModule();
    })();
  }
  return _modulePromise;
}

/**
 * Recursively create directories in Emscripten MEMFS.
 */
function mkdirp(FS, path) {
  const parts = path.split('/').filter(Boolean);
  let current = '';
  for (const part of parts) {
    current += '/' + part;
    try {
      FS.mkdir(current);
    } catch (_) {
      // already exists
    }
  }
}

/**
 * Recursively remove a directory tree from Emscripten MEMFS.
 */
function rmrf(FS, path) {
  try {
    const entries = FS.readdir(path).filter(e => e !== '.' && e !== '..');
    for (const entry of entries) {
      const full = path + '/' + entry;
      const stat = FS.stat(full);
      if (FS.isDir(stat.mode)) {
        rmrf(FS, full);
      } else {
        FS.unlink(full);
      }
    }
    FS.rmdir(path);
  } catch (_) {
    // path doesn't exist, nothing to clean
  }
}

/**
 * Convert ODB++ files to KiCad .kicad_pcb format.
 *
 * @param {Array<{path: string, content: string}>} files
 *   Array of file entries representing the ODB++ directory tree.
 *   Each entry has a relative path (e.g. "matrix/matrix") and string content.
 * @returns {Promise<{success: boolean, kicadPcb?: string, error?: string}>}
 */
export async function convertOdb(files) {
  const Module = await getModule();
  const FS = Module.FS;
  const mountPoint = '/odb';

  // Clean up any previous run
  rmrf(FS, mountPoint);
  mkdirp(FS, mountPoint);

  // Write input files into MEMFS
  for (const file of files) {
    const fullPath = mountPoint + '/' + file.path;
    const dir = fullPath.substring(0, fullPath.lastIndexOf('/'));
    mkdirp(FS, dir);
    FS.writeFile(fullPath, file.content);
  }

  // Run conversion
  let raw;
  try {
    raw = Module.convertOdb(mountPoint);
  } finally {
    // Clean up MEMFS
    rmrf(FS, mountPoint);
  }

  // Parse result — the C++ binding returns "ERROR: ..." on failure
  if (raw.startsWith('ERROR: ')) {
    return {
      success: false,
      error: raw.substring(7),
    };
  }

  return {
    success: true,
    kicadPcb: raw,
  };
}
