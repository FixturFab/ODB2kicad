/**
 * A file entry in an ODB++ directory tree.
 */
export interface OdbFile {
  /** Relative path within the ODB++ directory, e.g. "matrix/matrix" */
  path: string;
  /** File content as a string */
  content: string;
}

/**
 * Result of an ODB++ to KiCad conversion.
 */
export interface ConvertResult {
  /** Whether the conversion succeeded */
  success: boolean;
  /** The .kicad_pcb s-expression output (present when success is true) */
  kicadPcb?: string;
  /** Error message (present when success is false) */
  error?: string;
}

/**
 * Convert ODB++ files to KiCad .kicad_pcb format.
 *
 * Accepts an array of file entries representing the ODB++ directory tree.
 * Files are loaded into Emscripten's in-memory filesystem, converted,
 * and the result is returned as a string.
 *
 * @param files - Array of {path, content} entries
 * @returns Conversion result with .kicad_pcb output or error message
 */
export declare function convertOdb(files: OdbFile[]): Promise<ConvertResult>;

/**
 * Convert an ODB++ archive (.tgz, .tar.gz, .zip, or .tar) to KiCad .kicad_pcb format.
 *
 * The archive is extracted in-memory and the contained files are passed to convertOdb().
 * A common root directory prefix is automatically stripped (e.g., "odb-output/matrix/matrix"
 * becomes "matrix/matrix").
 *
 * @param buffer - Archive data as ArrayBuffer or Uint8Array
 * @returns Conversion result with .kicad_pcb output or error message
 */
export declare function convertOdbArchive(buffer: ArrayBuffer | Uint8Array): Promise<ConvertResult>;
