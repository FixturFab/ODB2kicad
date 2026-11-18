#include "odb_parser/tools_parser.h"
#include "util/odb_file.h"
#include "util/string_utils.h"
#include <cstdlib>
#include <fstream>

namespace odb2kicad {

bool parseDrillTools(const std::string& path, OdbDrillTools& tools) {
    auto opened = openOdbFile(path);  // the file, or its compressed twin <path>.z
    if (!opened) return false;
    std::istream& file = *opened;
    tools.present = true;

    std::string line;
    bool inTool = false;
    OdbDrillTool cur;
    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;
        if (startsWith(line, "TOOLS") && line.find('{') != std::string::npos) {
            inTool = true;
            cur = OdbDrillTool();
            continue;
        }
        if (line == "}") {
            if (inTool && cur.num >= 0) tools.tools.push_back(cur);
            inTool = false;
            continue;
        }
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = toUpper(trim(line.substr(0, eq)));
        std::string val = trim(line.substr(eq + 1));
        auto num = [&val]() { return std::strtod(val.c_str(), nullptr); };  // "" and text read as 0
        if (!inTool) {
            if (key == "UNITS") tools.units = val;
            else if (key == "THICKNESS") tools.thickness = num();
            else if (key == "USER_PARAMS") tools.userParams = val;
            continue;
        }
        if (key == "NUM") cur.num = val.empty() ? -1 : std::atoi(val.c_str());
        else if (key == "TYPE") cur.type = toUpper(val);
        else if (key == "TYPE2") cur.type2 = toUpper(val);
        else if (key == "BIT") cur.bit = val;
        else if (key == "MIN_TOL") cur.minTol = num();
        else if (key == "MAX_TOL") cur.maxTol = num();
        else if (key == "FINISH_SIZE") cur.finishSize = num();
        else if (key == "DRILL_SIZE") cur.drillSize = num();
    }
    return true;
}

} // namespace odb2kicad
