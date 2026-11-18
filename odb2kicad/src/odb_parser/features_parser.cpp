#include "odb_parser/features_parser.h"
#include "odb_parser/symbols.h"
#include "odb_parser/contour.h"
#include "util/odb_file.h"
#include "util/string_utils.h"
#include <fstream>
#include <sstream>

namespace odb2kicad {

// Symbol table indices are dense from 0 (a job uses hundreds, rarely thousands)
static const int kMaxSymbolIndex = 1 << 20;

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
            // Only numeric attribute keys; a named one ("ID=472680", written by some exporters
            // after a comma rather than a ';') is skipped
            int idx;
            if (parseInt(part.substr(0, eq), idx)) result[idx] = part.substr(eq + 1);
        }
        else {
            // A bare index is a boolean attribute that is set (.smd, .test_point)
            int idx;
            if (parseInt(part, idx)) result[idx] = "1";
        }
    }
    return result;
}

bool parseFeatures(const std::string& path, OdbFeatures& features) {
    auto opened = openOdbFile(path);  // the file, or its compressed twin <path>.z
    if (!opened) return false;
    std::istream& file = *opened;

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
            int idx;
            // an index that is not a number, negative, or absurdly large (it sizes the table) is
            // a damaged line: skipped, and features naming it draw nothing
            if (sp != std::string::npos && parseInt(line.substr(1, sp - 1), idx) && idx >= 0 && idx < kMaxSymbolIndex) {
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
            int idx;
            if (sp != std::string::npos && parseInt(line.substr(1, sp - 1), idx)) {
                features.attrDefs[idx] = trim(line.substr(sp + 1));
            }
            continue;
        }

        // String definition: &idx text
        if (line[0] == '&') {
            auto sp = line.find(' ');
            int idx;
            if (sp != std::string::npos && parseInt(line.substr(1, sp - 1), idx)) {
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
            iss >> pad.x >> pad.y >> pad.symIdx >> pad.polarity >> pad.dcode >> pad.mirror >> pad.angle;
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

        // Arc: A xs ys xe ye xc yc sym_idx [polarity] [dcode] [clockwise Y|N]
        // Only the six coordinates and the symbol are required; polarity, dcode and the direction
        // may be missing. kicad-cli (and others) write the direction, which is honoured; an arc
        // without it runs counter-clockwise, its centre on the left of the way from start to end.
        if (line[0] == 'A' && line.size() > 1 && line[1] == ' ') {
            std::istringstream iss(mainPart.substr(2));
            OdbArc arc;
            iss >> arc.xs >> arc.ys >> arc.xe >> arc.ye >> arc.xc >> arc.yc >> arc.symIdx;
            std::vector<std::string> rest;
            for (std::string tok; iss >> tok;) rest.push_back(toUpper(tok));
            size_t i = 0;
            arc.polarity = "P";
            if (i < rest.size() && (rest[i] == "P" || rest[i] == "N")) arc.polarity = rest[i++];
            if (i < rest.size() && rest[i].find_first_not_of("0123456789+-.") == std::string::npos) i++;  // dcode
            if (i < rest.size() && (rest[i] == "Y" || rest[i] == "N")) {
                arc.clockwise = rest[i] == "Y";
                arc.directionGiven = true;
            }
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

        // Text: T <x> <y> <font> <pol> <orient> [<angle>] <xsize> <ysize> <width factor> '<text>' <version>
        // (V: the same, drawn downwards). The string is quoted and may hold blanks and ';', so
        // the record is cut at its quotes, not at the first ';'.
        if ((line[0] == 'T' || line[0] == 'V') && line.size() > 1 && line[1] == ' ') {
            OdbText t;
            t.featIdx = featCount++;
            t.vertical = line[0] == 'V';
            auto q1 = line.find('\''), q2 = line.rfind('\'');
            std::string head = q1 == std::string::npos ? mainPart.substr(2) : line.substr(2, q1 - 2);
            std::string tail;
            if (q1 != std::string::npos && q2 > q1) {
                t.text = line.substr(q1 + 1, q2 - q1 - 1);
                tail = line.substr(q2 + 1);
            }
            std::istringstream iss(head);
            int orient = 0;
            iss >> t.x >> t.y >> t.font >> t.polarity >> orient;
            if (orient == 8 || orient == 9) iss >> t.angle;
            else t.angle = 90.0 * (orient % 4);
            t.mirror = orient == 9 || (orient >= 4 && orient <= 7);
            iss >> t.xsize >> t.ysize >> t.weight;
            if (q1 == std::string::npos) iss >> t.text;  // an unquoted single word
            auto tsemi = tail.find(';');
            std::istringstream vs(tsemi == std::string::npos ? tail : tail.substr(0, tsemi));
            vs >> t.version;
            if (tsemi != std::string::npos) t.attrs = parseAttrs(tail.substr(tsemi + 1));
            if (!iss.fail() || !t.text.empty()) features.texts.push_back(t);
            continue;
        }

        // Barcode records are not converted but still take a feature index
        if (line[0] == 'B' && line.size() > 1 && line[1] == ' ') {
            featCount++;
            features.barcodes++;
            continue;
        }

        // Surface contour records (OB / OS / OC / OE)
        if (parseContourRecord(line, currentSurface, currentContour, contourTolerance(features.units))) continue;
        if (line == "SE") { currentSurface = nullptr; continue; }
    }
    return true;
}

} // namespace odb2kicad
