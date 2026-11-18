// WASM bindings for odb2kicad (Emscripten embind). The JS wrapper is wasm/src/index.mjs.

#include <emscripten/bind.h>
#include "odb2kicad_lib.h"

// What the last conversion said besides its board: report lines and step names, one per line
static std::string gLastReport, gLastSteps, gLastStep;

static std::string joinLines(const std::vector<std::string>& lines) {
    std::string out;
    for (auto& l : lines) out += l + "\n";
    return out;
}

// The named step ("" = the job's first step). documentLimit: features a document layer may hold
// to be drawn (-1 any, 0 none); the wrapper passes ConvertOptions' default when the caller gives none.
static std::string convert_odb_options(const std::string& odbDirPath, const std::string& step, double documentLimit) {
    odb2kicad::ConvertOptions options;
    options.step = step;
    options.documentFeatureLimit = (long long)documentLimit;
    auto result = odb2kicad::convert(odbDirPath, options);
    gLastReport = joinLines(result.report);
    gLastSteps = joinLines(result.stepNames);
    gLastStep = result.step;
    if (!result.success) {
        return "ERROR: " + result.error;
    }
    return result.kicad_pcb;
}

static std::string convert_odb_step(const std::string& odbDirPath, const std::string& step) {
    return convert_odb_options(odbDirPath, step, (double)odb2kicad::ConvertOptions().documentFeatureLimit);
}

static std::string convert_odb(const std::string& odbDirPath) {
    return convert_odb_step(odbDirPath, "");
}

static std::string last_report() { return gLastReport; }
static std::string last_steps() { return gLastSteps; }
static std::string last_step() { return gLastStep; }

EMSCRIPTEN_BINDINGS(odb2kicad) {
    emscripten::function("convertOdb", &convert_odb);
    emscripten::function("convertOdbStep", &convert_odb_step);
    emscripten::function("convertOdbWith", &convert_odb_options);
    emscripten::function("lastReport", &last_report);
    emscripten::function("lastSteps", &last_steps);
    emscripten::function("lastStep", &last_step);
}
