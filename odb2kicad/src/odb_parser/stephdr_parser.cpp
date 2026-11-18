#include "odb_parser/stephdr_parser.h"
#include "util/odb_file.h"
#include "util/string_utils.h"
#include <cstdlib>
#include <fstream>

namespace odb2kicad {

bool parseStepHeader(const std::string& path, OdbStepHeader& header) {
    auto opened = openOdbFile(path);  // the file, or its compressed twin <path>.z
    if (!opened) return false;
    std::istream& file = *opened;

    std::string line;
    OdbStepRepeat* block = nullptr;   // inside a STEP-REPEAT { ... } block
    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;

        if (startsWith(line, "STEP-REPEAT")) {
            header.stepRepeats++;
            header.repeats.emplace_back();
            block = &header.repeats.back();
            continue;
        }
        if (line[0] == '}') { block = nullptr; continue; }
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = toUpper(trim(line.substr(0, eq)));
        std::string val = trim(line.substr(eq + 1));
        // a value that is not a number reads as 0 (an empty X_ORIGIN= is common)
        auto num = [&val]() { return std::strtod(val.c_str(), nullptr); };
        auto yes = [&val]() { std::string v = toUpper(val); return v == "YES" || v == "Y" || v == "1"; };

        if (block) {
            if (key == "NAME") block->name = val;
            else if (key == "X") block->x = num();
            else if (key == "Y") block->y = num();
            else if (key == "DX") block->dx = num();
            else if (key == "DY") block->dy = num();
            else if (key == "NX") block->nx = (int)num();
            else if (key == "NY") block->ny = (int)num();
            else if (key == "ANGLE") block->angle = num();
            else if (key == "MIRROR") block->mirror = yes();
            else if (key == "FLIP") block->flip = yes();
            continue;
        }
        if (key == "X_ORIGIN") header.xOrigin = num();
        else if (key == "Y_ORIGIN") header.yOrigin = num();
        else if (key == "X_DATUM") header.xDatum = num();
        else if (key == "Y_DATUM") header.yDatum = num();
        else if (key == "UNITS") header.units = val;
    }
    return true;
}

} // namespace odb2kicad
