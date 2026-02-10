#include "odb_parser/stephdr_parser.h"
#include "util/string_utils.h"
#include <fstream>

namespace odb2kicad {

bool parseStepHeader(const std::string& path, OdbStepHeader& header) {
    std::ifstream file(path);
    if (!file.is_open()) return false;

    std::string line;
    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;

        auto eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);

        if (key == "X_ORIGIN") header.xOrigin = val.empty() ? 0 : std::stod(val);
        else if (key == "Y_ORIGIN") header.yOrigin = val.empty() ? 0 : std::stod(val);
        else if (key == "X_DATUM") header.xDatum = val.empty() ? 0 : std::stod(val);
        else if (key == "Y_DATUM") header.yDatum = val.empty() ? 0 : std::stod(val);
        else if (key == "UNITS") header.units = val;
    }
    return true;
}

} // namespace odb2kicad
