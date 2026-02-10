#include "odb_parser/netlist_parser.h"
#include "util/string_utils.h"
#include <fstream>
#include <sstream>

namespace odb2kicad {

bool parseNetlist(const std::string& path, OdbNetlist& netlist) {
    std::ifstream file(path);
    if (!file.is_open()) return false;

    std::string line;
    bool inPoints = false;

    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') {
            if (line.find("Netlist points") != std::string::npos)
                inPoints = true;
            continue;
        }

        if (startsWith(line, "H ")) continue;

        // Net definition: $idx name
        if (line[0] == '$') {
            std::istringstream iss(line.substr(1));
            OdbNetDef nd;
            iss >> nd.index >> nd.name;
            netlist.nets.push_back(nd);
            continue;
        }

        // Net points (lines starting with a digit)
        if (inPoints && !line.empty() && (std::isdigit(line[0]) || line[0] == '-')) {
            std::istringstream iss(line);
            OdbNetPoint pt;
            std::string layerStr;
            iss >> pt.netIdx >> pt.viaFlag >> pt.x >> pt.y >> layerStr >> pt.padW >> pt.padH;
            pt.layer = layerStr;
            // Check if "v" flag at end
            std::string rest;
            while (iss >> rest) {
                if (rest == "v") pt.isVia = true;
            }
            netlist.points.push_back(pt);
            continue;
        }
    }
    return true;
}

} // namespace odb2kicad
