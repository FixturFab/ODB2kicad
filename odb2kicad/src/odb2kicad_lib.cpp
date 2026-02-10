#include "odb2kicad_lib.h"
#include "odb_parser/odb_parser.h"
#include "writer/kicad_writer.h"
#include <sstream>

namespace odb2kicad {

ConvertResult convert(const std::string& odbDirPath) {
    ConvertResult result;

    // Parse ODB++ directory
    OdbDesign design;
    if (!parseOdbDirectory(odbDirPath, design)) {
        result.success = false;
        result.error = "Failed to parse ODB++ directory: " + odbDirPath;
        return result;
    }

    // Transform to KiCad model
    KicadPcb pcb = transformToKicad(design);

    // Write to string
    std::ostringstream oss;
    writeKicadPcb(oss, pcb);

    result.success = true;
    result.kicad_pcb = oss.str();
    return result;
}

} // namespace odb2kicad
