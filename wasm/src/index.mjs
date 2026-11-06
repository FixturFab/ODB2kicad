// odb2kicad-wasm — High-level JavaScript API
// Wraps the Emscripten WASM module with a clean convertOdb() interface.

import { gunzipSync, unzipSync } from 'fflate';

let _modulePromise = null;

/**
 * Lazily initialize and cache the WASM module.
 * @returns {Promise<object>} The initialized Emscripten module.
 */
function getModule() {
  if (!_modulePromise) {
    _modulePromise = (async () => {
      const wasmModuleUrl = new URL('./odb2kicad_wasm.mjs', import.meta.url).href;
      const initModule = (await import(wasmModuleUrl)).default;
      return await initModule();
    })();
    // a failed init is not cached: the next call retries
    _modulePromise.catch(() => { _modulePromise = null; });
  }
  return _modulePromise;
}

/**
 * Drop the cached WASM module so the next conversion starts a fresh instance. Done after any
 * abort or throw out of the WASM: an aborted Emscripten runtime is unusable afterwards.
 */
export function resetWasmModule() {
  _modulePromise = null;
}

// A failed result: `code` says which kind of failure, `error` says what to do about it.
//   bad-input   the call itself (not an array of files, not an ArrayBuffer/Uint8Array)
//   archive     the archive could not be unpacked, or holds no files
//   not-odb     no matrix/matrix: not an ODB++ job (the C++ also says no-input / bad-matrix /
//               no-step / no-layers for jobs it cannot read)
//   crash       the converter stopped (WASM abort, out of memory); the module was reset
//   internal    anything else the converter reported
function fail(code, error) {
  return { success: false, code, error };
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
  if (!Array.isArray(files)) return fail('bad-input', 'convertOdb expects an array of {path, content} entries');
  if (files.length === 0) return fail('not-odb', 'no files given: an ODB++ job needs at least matrix/matrix and a steps/ folder');
  if (!files.some((f) => /(^|\/)matrix\/matrix$/.test(f.path))) {
    const top = [...new Set(files.map((f) => f.path.split('/')[0]))].slice(0, 8).join(', ');
    return fail('not-odb', `no matrix/matrix among ${files.length} files (top level: ${top}): not an ODB++ job`);
  }
  let Module;
  try {
    Module = await getModule();
  } catch (err) {
    return fail('crash', 'the converter module could not start: ' + (err && err.message ? err.message : String(err)));
  }
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

  // Run conversion. A WASM abort (a crash, out of memory) throws; the runtime is unusable after
  // it, so the module is dropped and the next call starts afresh.
  let raw;
  try {
    raw = Module.convertOdb(mountPoint);
  } catch (err) {
    resetWasmModule();
    const what = err && err.message ? err.message : String(err);
    const oom = /memory|Cannot enlarge/i.test(what);
    return fail('crash', `the converter stopped (${oom ? 'out of memory' : 'WASM abort'}: ${what.slice(0, 200)}) ` +
      `after reading ${files.length} files; the module was reset, so the next conversion starts clean`);
  } finally {
    try { rmrf(FS, mountPoint); } catch (_) { /* an aborted runtime may refuse */ }
  }

  // Parse result: the C++ binding returns "ERROR: <code>: <reason>" on failure
  if (raw.startsWith('ERROR: ')) {
    const msg = raw.substring(7);
    const m = /^([a-z-]+): ([\s\S]*)$/.exec(msg);
    return m ? fail(m[1], m[2]) : fail('internal', msg);
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
  let longName = null;  // GNU 'L' or pax 'x' path for the next entry (names over 100 chars)

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

    if (type === 0x4c /* 'L' GNU long name */ || type === 0x78 /* 'x' pax header */) {
      const body = decoder.decode(data.subarray(offset, offset + size));
      if (type === 0x4c) {
        longName = body.replace(/\0.*$/s, '');
      } else {
        const m = body.match(/(?:^|\n)\d+ path=([^\n]*)\n/);
        if (m) longName = m[1];
      }
      offset += Math.ceil(size / 512) * 512;
      continue;
    }
    if (longName !== null) {
      name = longName;
      longName = null;
    }

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
    return fail('bad-input', 'convertOdbArchive expects an ArrayBuffer or Uint8Array');
  }

  let files;
  try {
    files = extractArchive(data);
  } catch (err) {
    return fail('archive', 'the archive could not be unpacked (' + err.message + '); send a .tgz, .tar.gz, .zip or .tar of the ODB++ job');
  }

  // Drop macOS zip junk, then anchor on the ODB++ `matrix/matrix` file and rebase
  // every path to its grandparent dir, so archives with files beside the job convert.
  files = files.filter(f => !/(^|\/)(__MACOSX\/|\.DS_Store$|\._)/.test(f.path));
  const anchor = files.find(f => /(^|\/)matrix\/matrix$/.test(f.path));
  if (anchor) {
    const root = anchor.path.replace(/matrix\/matrix$/, '');
    files = files
      .filter(f => f.path.startsWith(root))
      .map(f => ({ path: f.path.slice(root.length), content: f.content }))
      .filter(f => f.path.length > 0);
  } else {
    files = stripCommonPrefix(files);
  }

  if (files.length === 0) {
    return fail('archive', 'the archive holds no files');
  }

  return convertOdb(files);
}
