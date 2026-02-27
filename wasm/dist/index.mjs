// odb2kicad-wasm — High-level JavaScript API
// Wraps the Emscripten WASM module with a clean convertOdb() interface.

import { gunzipSync, unzipSync } from 'fflate';
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

/**
 * Parse a tar archive (uncompressed) into an array of {path, content} entries.
 * Handles POSIX ustar and GNU tar formats. Skips directories and special entries.
 * @param {Uint8Array} data — raw tar bytes
 * @returns {Array<{path: string, content: string}>}
 */
function parseTar(data) {
  const files = [];
  const decoder = new TextDecoder();
  let offset = 0;

  while (offset + 512 <= data.length) {
    const header = data.subarray(offset, offset + 512);

    // Check for end-of-archive (two consecutive zero blocks)
    if (header.every(b => b === 0)) break;

    // Read file name from header (bytes 0-99, null-terminated)
    let name = decoder.decode(header.subarray(0, 100)).replace(/\0.*/, '');

    // Check for POSIX ustar prefix (bytes 345-499)
    const magic = decoder.decode(header.subarray(257, 263)).replace(/\0.*/, '');
    if (magic === 'ustar') {
      const prefix = decoder.decode(header.subarray(345, 500)).replace(/\0.*/, '');
      if (prefix) name = prefix + '/' + name;
    }

    // Read file size (octal, bytes 124-135)
    const sizeStr = decoder.decode(header.subarray(124, 136)).replace(/\0.*/, '').trim();
    const size = parseInt(sizeStr, 8) || 0;

    // Read file type (byte 156): '0' or '\0' = regular file, '5' = directory
    const type = header[156];

    offset += 512; // move past header

    if ((type === 0x30 || type === 0) && size > 0 && name) {
      // Regular file — extract content as string
      const content = decoder.decode(data.subarray(offset, offset + size));
      files.push({ path: name, content });
    }

    // Advance past file data, rounded up to 512-byte boundary
    offset += Math.ceil(size / 512) * 512;
  }

  return files;
}

/**
 * Detect archive format and extract files.
 * @param {Uint8Array} data — raw archive bytes
 * @returns {Array<{path: string, content: string}>}
 */
function extractArchive(data) {
  // Check for gzip magic bytes (1f 8b)
  if (data[0] === 0x1f && data[1] === 0x8b) {
    const tarData = gunzipSync(data);
    return parseTar(tarData);
  }

  // Check for ZIP magic bytes (PK\x03\x04)
  if (data[0] === 0x50 && data[1] === 0x4b && data[2] === 0x03 && data[3] === 0x04) {
    const entries = unzipSync(data);
    const files = [];
    const decoder = new TextDecoder();
    for (const [name, content] of Object.entries(entries)) {
      // Skip directories (names ending with /)
      if (name.endsWith('/')) continue;
      files.push({ path: name, content: decoder.decode(content) });
    }
    return files;
  }

  // Try as uncompressed tar (check for "ustar" at offset 257)
  if (data.length > 262) {
    const magic = new TextDecoder().decode(data.subarray(257, 263)).replace(/\0.*/, '');
    if (magic === 'ustar') {
      return parseTar(data);
    }
  }

  throw new Error('Unrecognized archive format. Expected .tgz, .tar.gz, .zip, or .tar');
}

/**
 * Strip the common root directory prefix from extracted file paths.
 * ODB++ archives typically contain a single top-level directory.
 * @param {Array<{path: string, content: string}>} files
 * @returns {Array<{path: string, content: string}>}
 */
function stripCommonPrefix(files) {
  if (files.length === 0) return files;

  // Find common prefix
  const paths = files.map(f => f.path);
  const firstParts = paths[0].split('/');
  let commonDepth = 0;

  for (let i = 0; i < firstParts.length - 1; i++) {
    const prefix = firstParts.slice(0, i + 1).join('/') + '/';
    if (paths.every(p => p.startsWith(prefix))) {
      commonDepth = i + 1;
    } else {
      break;
    }
  }

  if (commonDepth === 0) return files;

  const prefixLen = firstParts.slice(0, commonDepth).join('/').length + 1;
  return files.map(f => ({
    path: f.path.substring(prefixLen),
    content: f.content,
  })).filter(f => f.path.length > 0);
}

/**
 * Convert an ODB++ archive (.tgz, .tar.gz, .zip, or .tar) to KiCad .kicad_pcb format.
 *
 * The archive is extracted in-memory and the contained files are passed to convertOdb().
 * A common root directory prefix is automatically stripped (e.g., "odb-output/matrix/matrix"
 * becomes "matrix/matrix").
 *
 * @param {ArrayBuffer|Uint8Array} buffer — archive data
 * @returns {Promise<{success: boolean, kicadPcb?: string, error?: string}>}
 */
export async function convertOdbArchive(buffer) {
  let data;
  if (buffer instanceof ArrayBuffer) {
    data = new Uint8Array(buffer);
  } else if (buffer instanceof Uint8Array) {
    data = buffer;
  } else {
    return { success: false, error: 'Expected ArrayBuffer or Uint8Array' };
  }

  let files;
  try {
    files = extractArchive(data);
  } catch (err) {
    return { success: false, error: 'Archive extraction failed: ' + err.message };
  }

  files = stripCommonPrefix(files);

  if (files.length === 0) {
    return { success: false, error: 'Archive contains no files' };
  }

  return convertOdb(files);
}
