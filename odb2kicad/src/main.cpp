#include "odb_parser/odb_parser.h"
#include "writer/kicad_writer.h"
#include <fstream>
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: odb2kicad <odb-directory> [output.kicad_pcb]\n";
        std::cerr << "  If output is omitted, writes to stdout.\n";
        return 1;
    }

    std::string odbDir = argv[1];
    std::string outputPath = (argc >= 3) ? argv[2] : "";

    // Parse ODB++ directory
    odb2kicad::OdbDesign design;
    if (!odb2kicad::parseOdbDirectory(odbDir, design)) {
        std::cerr << "Error: failed to parse ODB++ directory: " << odbDir << "\n";
        return 1;
    }

    // Transform to KiCad model
    odb2kicad::KicadPcb pcb = odb2kicad::transformToKicad(design);

    // Write output
    if (outputPath.empty()) {
        odb2kicad::writeKicadPcb(std::cout, pcb);
    } else {
        std::ofstream outFile(outputPath);
        if (!outFile.is_open()) {
            std::cerr << "Error: cannot open output file: " << outputPath << "\n";
            return 1;
        }
        odb2kicad::writeKicadPcb(outFile, pcb);
        std::cerr << "Written: " << outputPath << "\n";
    }

    return 0;
}
