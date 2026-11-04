#pragma once
#include <string>
#include <utility>
#include <vector>

namespace odb2kicad {

struct ConvertResult {
    bool success;
    std::string kicad_pcb;   // output s-expression on success
    std::string error;       // on failure: "<code>: <reason>" (codes in odb_parser.h, or internal)
};

// Convert from a filesystem path (used by CLI and WASM-with-MEMFS)
ConvertResult convert(const std::string& odbDirPath);

} // namespace odb2kicad
