/**
 * A file entry in an ODB++ directory tree.
 */
export interface OdbFile {
  /** Relative path within the ODB++ directory, e.g. "matrix/matrix" */
  path: string;
  /**
   * File content: a string, or the file's bytes. Compressed ODB++ files (UNIX compress, named
   * "<file>.z" / "<file>.Z", e.g. "steps/pcb/layers/top/features.z") must be given as bytes.
   */
  content: string | Uint8Array;
}

/**
 * Result of an ODB++ to KiCad conversion.
 */
export interface ConvertResult {
  /** Whether the conversion succeeded */
  success: boolean;
  /** The .kicad_pcb s-expression output (present when success is true) */
  kicadPcb?: string;
  /** The step kicadPcb holds (present when success is true) */
  step?: string;
  /**
   * Every step of the job in conversion order: the matrix's STEP records by COL, then step
   * folders it does not name (present when success is true). A single-step job lists one;
   * convert another with `{ step }`.
   */
  steps?: string[];
  /** What the reader assumed, dropped or chose: "warning: ..." and "info: ..." lines */
  report?: string[];
  /** What went wrong and what to do about it (present when success is false) */
  error?: string;
  /**
   * Kind of failure (present when success is false): bad-input, archive, not-odb, no-input,
   * bad-matrix, no-step, no-layers, crash (WASM abort / out of memory; the module was reset),
   * internal.
   */
  code?: 'bad-input' | 'archive' | 'not-odb' | 'no-input' | 'bad-matrix' | 'no-step' | 'no-layers' | 'crash' | 'internal';
}

/** Options of a conversion. */
export interface ConvertOptions {
  /** Convert this step of a multi-step job (any case). Default: the job's first step. */
  step?: string;
  /**
   * Document layers that no KiCad layer answers to by name (assembly drawings, process maps) are
   * drawn on free User.1..User.9 layers when they hold at most this many features (default
   * 100000). 'all' draws them whatever their size, 'none' leaves them out. KiCad-made jobs are
   * not affected.
   */
  documentLayers?: 'all' | 'none' | number;
}

/** Drop the cached WASM module; the next conversion starts a fresh instance. */
export declare function resetWasmModule(): void;

/**
 * Convert ODB++ files to KiCad .kicad_pcb format.
 *
 * Accepts an array of file entries representing the ODB++ directory tree.
 * Files are loaded into Emscripten's in-memory filesystem, converted,
 * and the result is returned as a string.
 *
 * @param files - Array of {path, content} entries
 * @param options - Which step to convert (default: the first)
 * @returns Conversion result with .kicad_pcb output or error message
 */
export declare function convertOdb(files: OdbFile[], options?: ConvertOptions): Promise<ConvertResult>;

/**
 * Convert an ODB++ archive (.tgz, .tar.gz, .zip, or .tar) to KiCad .kicad_pcb format.
 *
 * The archive is extracted in-memory and the contained files are passed to convertOdb().
 * A common root directory prefix is automatically stripped (e.g., "odb-output/matrix/matrix"
 * becomes "matrix/matrix").
 *
 * @param buffer - Archive data as ArrayBuffer or Uint8Array
 * @param options - Which step to convert (default: the first)
 * @returns Conversion result with .kicad_pcb output or error message
 */
export declare function convertOdbArchive(buffer: ArrayBuffer | Uint8Array, options?: ConvertOptions): Promise<ConvertResult>;
