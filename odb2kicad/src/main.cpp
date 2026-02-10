#include "odb2kicad_lib.h"
#include <fstream>
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: odb2kicad <odb-directory> [output.kicad_pcb]\n";
        std::cerr << "  If output is omitted, writes to stdout.\n";
        return 1;
    }

    auto result = odb2kicad::convert(argv[1]);
    if (!result.success) {
        std::cerr << "Error: " << result.error << "\n";
        return 1;
    }

    if (argc >= 3) {
        std::ofstream outFile(argv[2]);
        if (!outFile.is_open()) {
            std::cerr << "Error: cannot open output file: " << argv[2] << "\n";
            return 1;
        }
        outFile << result.kicad_pcb;
        std::cerr << "Written: " << argv[2] << "\n";
    } else {
        std::cout << result.kicad_pcb;
    }

    return 0;
}
