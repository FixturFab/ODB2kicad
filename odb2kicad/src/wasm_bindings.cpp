// WASM bindings for odb2kicad — Stage 3 will implement the full embind API.
// This placeholder allows the Emscripten CMake build to configure and compile.

#include <emscripten/bind.h>
#include "odb2kicad_lib.h"

static std::string convert_odb(const std::string& odbDirPath) {
    auto result = odb2kicad::convert(odbDirPath);
    if (!result.success) {
        return "ERROR: " + result.error;
    }
    return result.kicad_pcb;
}

EMSCRIPTEN_BINDINGS(odb2kicad) {
    emscripten::function("convertOdb", &convert_odb);
}
