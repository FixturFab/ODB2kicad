#include "odb_parser/features_parser.h"
#include "odb_parser/symbols.h"
#include "odb_parser/contour.h"
#include "util/string_utils.h"
#include <fstream>
#include <sstream>

namespace odb2kicad {

static std::map<int, std::string> parseAttrs(std::string attrStr) {
    std::map<int, std::string> result;
    // ";0=1,25=2,26;ID=516688": the numbered attributes end at the next ';' (ID= follows)
    auto semi = attrStr.find(';');
    if (semi != std::string::npos) attrStr = attrStr.substr(0, semi);
    if (attrStr.empty()) return result;

    auto parts = split(attrStr, ',');
    for (auto& part : parts) {
        part = trim(part);
        auto eq = part.find('=');
        if (eq != std::string::npos) {
            std::string key = part.substr(0, eq);
            // Only parse numeric attribute keys; skip named attrs (e.g. "ID=472680")
            try {
                int idx = std::stoi(key);
                result[idx] = part.substr(eq + 1);
            } catch (const std::invalid_argument&) {
                // Named attribute from non-KiCad ODB++ — skip
            }
        }
        else {
            // A bare index is a boolean attribute that is set (.smd, .test_point)
            try { result[std::stoi(part)] = "1"; } catch (const std::exception&) {}
        }
    }
    return result;
}

bool parseFeatures(const std::string& path, OdbFeatures& features) {
    std::ifstream file(path);
    if (!file.is_open()) return false;

    std::string line;
    int featCount = 0;  // feature ordinal: P/L/A/S/T/B records in file order (eda/data FID index)
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
                char unitOverride = 0;  // "$n name I|M"
                if (symName.size() > 2 && symName[symName.size() - 2] == ' ' &&
                    (symName.back() == 'I' || symName.back() == 'M')) {
                    unitOverride = symName.back();
                    symName = trim(symName.substr(0, symName.size() - 2));
                }
                OdbSymbol sym = decodeSymbol(symName);
                sym.unitOverride = unitOverride;
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
            pad.featIdx = featCount++;
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
            ln.featIdx = featCount++;
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
            arc.featIdx = featCount++;
            features.arcs.push_back(arc);
            continue;
        }

        // Surface: S polarity dcode
        if (line[0] == 'S' && line.size() > 1 && line[1] == ' ') {
            features.surfaces.emplace_back();
            currentSurface = &features.surfaces.back();
            currentSurface->featIdx = featCount++;
            auto parts = split(mainPart, ' ');
            if (parts.size() >= 2) currentSurface->polarity = parts[1];
            continue;
        }

        // Text and barcode records are not converted but still take a feature index
        if ((line[0] == 'T' || line[0] == 'B') && line.size() > 1 && line[1] == ' ') {
            featCount++;
            continue;
        }

        // Surface contour records (OB / OS / OC / OE)
        if (parseContourRecord(line, currentSurface, currentContour, contourTolerance(features.units))) continue;
        if (line == "SE") { currentSurface = nullptr; continue; }
    }
    return true;
}

} // namespace odb2kicad
