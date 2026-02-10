#pragma once
#include <string>
#include <utility>
#include <vector>

namespace odb2kicad {

struct ConvertResult {
    bool success;
    std::string kicad_pcb;   // output s-expression on success
    std::string error;       // error message on failure
};

// Convert from a filesystem path (used by CLI and WASM-with-MEMFS)
ConvertResult convert(const std::string& odbDirPath);

} // namespace odb2kicad
