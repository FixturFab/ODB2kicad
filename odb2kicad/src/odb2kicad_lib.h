#pragma once
#include <string>
#include <utility>
#include <vector>

namespace odb2kicad {

// The converter's version, as written into every board's (generator ...)
#define ODB2KICAD_VERSION "0.4.2"

// One converted step of a multi-step job
struct StepOutput {
    std::string step;        // the step's name (the matrix's, else its folder's)
    std::string kicad_pcb;
    std::vector<std::string> report;
};

struct ConvertOptions {
    std::string step;        // convert this step ("" = the job's first step)
    bool allSteps = false;   // convert every step into ConvertResult::steps
    // A step whose stephdr places other steps (STEP-REPEAT: a panel) is one board holding a
    // copy of each placed step per NX x NY position, turned by ANGLE about the step's datum; the
    // copies' nets are named Board_<n>-<net>. False: the panel without them, as before.
    bool placeSteps = true;
    // DOCUMENT layers that no KiCad layer answers to by name (assembly drawings, process maps)
    // are drawn on free User.1 .. User.9 layers when they hold at most this many features;
    // -1 draws them whatever their size, 0 leaves them all out. Not applied to KiCad-made jobs.
    long long documentFeatureLimit = 100000;
    // KiCad file format: 0 = KiCad 9 (padstacks: a via or through-hole pad sized per copper layer)
    // when the board needs it, else KiCad 8; 8 = always KiCad 8 (one size per via / pad: the
    // largest copper flash); 9 = always KiCad 9.
    int kicadFormat = 0;
};

struct ConvertResult {
    bool success;
    std::string kicad_pcb;   // output s-expression on success
    std::string error;       // on failure: "<code>: <reason>" (codes in odb_parser.h, or internal)
    // What the reader assumed, dropped or chose: "warning: ..." and "info: ..." lines
    std::vector<std::string> report;
    std::string jobName;     // misc/info JOB_NAME ("" when it has none)
    std::string step;        // the step kicad_pcb holds
    // Every step of the job in conversion order (the matrix's STEP records by COL, then folders
    // it does not name). A single-step job lists one.
    std::vector<std::string> stepNames;
    // With ConvertOptions::allSteps: every step that could be converted, the first one included
    // (kicad_pcb is then steps[0]'s). A step with no layer data is left out, with a warning.
    std::vector<StepOutput> steps;
};

// Convert from a filesystem path (used by CLI and WASM-with-MEMFS). By default the job's first
// step, as before; see ConvertOptions for another step or all of them.
ConvertResult convert(const std::string& odbDirPath, const ConvertOptions& options = {});

// File name for one step of a multi-step job: "<job>__<step>.kicad_pcb", both parts reduced to
// letters, digits, '-', '_', '.' and '+' (anything else becomes '_'); job defaults to "job".
std::string stepFileName(const std::string& jobName, const std::string& stepName);

} // namespace odb2kicad
