#include "odb2kicad_lib.h"
#include "odb_parser/odb_parser.h"
#include "writer/kicad_writer.h"
#include <exception>
#include <sstream>

namespace odb2kicad {

ConvertResult convert(const std::string& odbDirPath) {
    ConvertResult result;

    // Parse ODB++ directory
    OdbDesign design;
    std::string why;
    result.success = false;
    try {
        if (!parseOdbDirectory(odbDirPath, design, why)) {
            result.error = why;
            return result;
        }
        KicadPcb pcb = transformToKicad(design);
        std::ostringstream oss;
        writeKicadPcb(oss, pcb);
        result.kicad_pcb = oss.str();
    } catch (const std::exception& e) {  // native builds; the WASM build aborts instead (see wasm/src)
        result.error = std::string("internal: the converter failed: ") + e.what();
        return result;
    }
    result.success = true;
    return result;
}

} // namespace odb2kicad
