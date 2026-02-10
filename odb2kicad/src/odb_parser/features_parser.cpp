#include "odb_parser/features_parser.h"
#include "odb_parser/symbols.h"
#include "util/string_utils.h"
#include <fstream>
#include <sstream>

namespace odb2kicad {

static std::map<int, std::string> parseAttrs(const std::string& attrStr) {
    std::map<int, std::string> result;
    if (attrStr.empty()) return result;

    auto parts = split(attrStr, ',');
    for (auto& part : parts) {
        part = trim(part);
        auto eq = part.find('=');
        if (eq != std::string::npos) {
            int idx = std::stoi(part.substr(0, eq));
            result[idx] = part.substr(eq + 1);
        }
        // Single value like "1" means attr flag is set (used for .smd)
    }
    return result;
}

bool parseFeatures(const std::string& path, OdbFeatures& features) {
    std::ifstream file(path);
    if (!file.is_open()) return false;

    std::string line;
    OdbSurface* currentSurface = nullptr;
    OdbContour* currentContour = nullptr;

    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;

        if (startsWith(line, "UNITS=")) {
            features.units = line.substr(6);
            continue;
        }

        if (startsWith(line, "F ")) continue; // feature count

        // Symbol definition: $idx name
        if (line[0] == '$') {
            auto sp = line.find(' ');
            if (sp != std::string::npos) {
                int idx = std::stoi(line.substr(1, sp - 1));
                std::string symName = trim(line.substr(sp + 1));
                OdbSymbol sym = decodeSymbol(symName);
                if (idx >= (int)features.symbols.size())
                    features.symbols.resize(idx + 1);
                features.symbols[idx] = sym;
            }
            continue;
        }

        // Attribute definition: @idx name
        if (line[0] == '@') {
            auto sp = line.find(' ');
            if (sp != std::string::npos) {
                int idx = std::stoi(line.substr(1, sp - 1));
                features.attrDefs[idx] = trim(line.substr(sp + 1));
            }
            continue;
        }

        // String definition: &idx text
        if (line[0] == '&') {
            auto sp = line.find(' ');
            if (sp != std::string::npos) {
                int idx = std::stoi(line.substr(1, sp - 1));
                features.strings[idx] = trim(line.substr(sp + 1));
            }
            continue;
        }

        // Separate attrs from main record
        std::string mainPart = line;
        std::string attrPart;
        auto semi = line.find(';');
        if (semi != std::string::npos) {
            mainPart = line.substr(0, semi);
            attrPart = line.substr(semi + 1);
        }

        // Pad: P x y sym_idx polarity dcode mirror angle
        if (line[0] == 'P' && line.size() > 1 && line[1] == ' ') {
            std::istringstream iss(mainPart.substr(2));
            OdbPad pad;
            int dcode;
            iss >> pad.x >> pad.y >> pad.symIdx >> pad.polarity >> dcode >> pad.mirror >> pad.angle;
            pad.attrs = parseAttrs(attrPart);
            features.pads.push_back(pad);
            continue;
        }

        // Line: L x1 y1 x2 y2 sym_idx polarity dcode
        if (line[0] == 'L' && line.size() > 1 && line[1] == ' ') {
            std::istringstream iss(mainPart.substr(2));
            OdbLine ln;
            int dcode;
            iss >> ln.x1 >> ln.y1 >> ln.x2 >> ln.y2 >> ln.symIdx >> ln.polarity >> dcode;
            ln.attrs = parseAttrs(attrPart);
            features.lines.push_back(ln);
            continue;
        }

        // Arc: A xs ys xe ye xc yc sym_idx polarity dcode clockwise
        if (line[0] == 'A' && line.size() > 1 && line[1] == ' ') {
            std::istringstream iss(mainPart.substr(2));
            OdbArc arc;
            int dcode;
            std::string cw;
            iss >> arc.xs >> arc.ys >> arc.xe >> arc.ye >> arc.xc >> arc.yc
                >> arc.symIdx >> arc.polarity >> dcode >> cw;
            arc.clockwise = (cw == "Y");
            arc.attrs = parseAttrs(attrPart);
            features.arcs.push_back(arc);
            continue;
        }

        // Surface: S polarity dcode
        if (line[0] == 'S' && line.size() > 1 && line[1] == ' ') {
            features.surfaces.emplace_back();
            currentSurface = &features.surfaces.back();
            auto parts = split(mainPart, ' ');
            if (parts.size() >= 2) currentSurface->polarity = parts[1];
            continue;
        }

        // Surface contour records
        if (startsWith(line, "OB ")) {
            if (!currentSurface) continue;
            currentSurface->contours.emplace_back();
            currentContour = &currentSurface->contours.back();
            std::istringstream iss(line.substr(3));
            double x, y;
            std::string dir;
            iss >> x >> y >> dir;
            currentContour->isHole = (dir == "H");
            currentContour->points.push_back({x, y});
            continue;
        }

        if (startsWith(line, "OS ")) {
            if (!currentContour) continue;
            std::istringstream iss(line.substr(3));
            double x, y;
            iss >> x >> y;
            currentContour->points.push_back({x, y});
            continue;
        }

        if (line == "OE") { currentContour = nullptr; continue; }
        if (line == "SE") { currentSurface = nullptr; continue; }
    }
    return true;
}

} // namespace odb2kicad
