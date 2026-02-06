#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Load a KiCad PCB from in-memory content (s-expression format).
 * @param pcb_content  Null-terminated string of the .kicad_pcb file content.
 * @param length       Length of pcb_content in bytes (0 = use strlen).
 * @return 0 on success, non-zero error code on failure.
 */
int kicad_load_pcb(const char* pcb_content, size_t length);

/**
 * Run DRC checks on the loaded PCB.
 * Must call kicad_load_pcb() first.
 * @return Number of violations found, or -1 on error.
 */
int kicad_run_drc(void);

/**
 * Get DRC results as a JSON string.
 * The returned pointer is valid until the next call to kicad_run_drc() or kicad_cleanup().
 * @return JSON string following the schemas.kicad.org/drc.v1.json schema, or NULL on error.
 */
const char* kicad_get_drc_results(void);

/**
 * Free all resources (board, DRC engine, results).
 */
void kicad_cleanup(void);

#ifdef __cplusplus
}
#endif
