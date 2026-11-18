#include "odb_parser/netlist_parser.h"
#include "util/odb_file.h"
#include "util/string_utils.h"
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace odb2kicad {

// The whole token as a number ("e", "staggered" and "1.2x" are not)
static bool parseNumber(const std::string& tok, double& out) {
    if (tok.empty()) return false;
    char* end = nullptr;
    out = std::strtod(tok.c_str(), &end);
    return end == tok.c_str() + tok.size();
}

bool parseNetlist(const std::string& path, OdbNetlist& netlist) {
    auto opened = openOdbFile(path);  // the file, or its compressed twin <path>.z
    if (!opened) return false;
    std::istream& file = *opened;

    std::string line;
    bool inPoints = false;

    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') {
            if (line.find("Netlist points") != std::string::npos)
                inPoints = true;
            continue;
        }

        if (startsWith(line, "UNITS=")) { netlist.units = trim(line.substr(6)); continue; }
        if (startsWith(line, "H ")) continue;

        // Net definition: $idx name
        if (line[0] == '$') {
            std::istringstream iss(line.substr(1));
            OdbNetDef nd;
            iss >> nd.index >> nd.name;
            netlist.nets.push_back(nd);
            continue;
        }

        // Net point: <net> <radius> <x> <y> <side> [<w> <h>] <epoint> <exp> [staggered sx sy sr] [v] ...
        // <w> <h> are written only for a pad without a hole (radius 0); <epoint> (e | m) and
        // <exp> (e | c | p | s) are letters, so the fields after <side> are read as tokens, not
        // numbers. A trailing "v" marks a via point.
        if (inPoints && (std::isdigit((unsigned char)line[0]) || line[0] == '-')) {
            std::istringstream iss(line);
            std::vector<std::string> tok;
            for (std::string t; iss >> t;) tok.push_back(t);
            if (tok.size() < 5) continue;
            double v[4];
            bool ok = true;
            for (int i = 0; i < 4 && ok; i++) ok = parseNumber(tok[i], v[i]);
            if (!ok) continue;
            OdbNetPoint pt;
            pt.netIdx = (int)v[0];
            pt.radius = v[1];
            pt.x = v[2];
            pt.y = v[3];
            pt.layer = tok[4];
            size_t k = 5;
            double w = 0, h = 0;
            if (k + 1 < tok.size() && parseNumber(tok[k], w) && parseNumber(tok[k + 1], h)) {
                pt.hasPadSize = true;
                pt.padW = w;
                pt.padH = h;
                k += 2;
            }
            if (k < tok.size()) pt.epoint = tok[k++];
            if (k < tok.size()) pt.exp = tok[k++];
            for (; k < tok.size(); k++)
                if (tok[k] == "v") pt.isVia = true;
            netlist.points.push_back(pt);
            continue;
        }
    }
    return true;
}

} // namespace odb2kicad
