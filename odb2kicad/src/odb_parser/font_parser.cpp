#include "odb_parser/font_parser.h"
#include "util/odb_file.h"
#include "util/string_utils.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace odb2kicad {

static bool readNumbers(const std::string& line, size_t skipWords, std::vector<double>& out) {
    std::istringstream iss(line);
    std::string tok;
    for (size_t i = 0; i < skipWords; i++) iss >> tok;
    while (iss >> tok) {
        char* end = nullptr;
        double v = std::strtod(tok.c_str(), &end);
        if (end == tok.c_str() || *end != 0) return false;
        out.push_back(v);
    }
    return true;
}

bool parseFont(const std::string& path, OdbFont& font, std::vector<int>* badLines) {
    auto opened = openOdbFile(path);  // the file, or its compressed twin <path>.z
    if (!opened) return false;
    std::istream& file = *opened;
    std::string line;
    bool inChar = false, inPolygon = false;
    char cur = 0;
    int lineNo = 0;
    auto bad = [&]() { if (badLines) badLines->push_back(lineNo); };
    while (std::getline(file, line)) {
        lineNo++;
        // "CHAR  " would be a blank's glyph: keep the character before trimming the line
        std::string raw = line;
        while (!raw.empty() && (raw.back() == '\r' || raw.back() == '\n')) raw.pop_back();
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;
        std::istringstream iss(line);
        std::string rec;
        iss >> rec;
        std::string key = toUpper(rec);
        // UNITS=MM, UNITS = MM or UNITS MM: the font's own units, whatever the layer files use
        if (startsWith(key, "UNITS")) {
            std::string v = line.substr(5);
            v.erase(0, v.find_first_not_of(" \t="));
            font.units = toUpper(trim(v));
            if (font.units != "MM" && font.units != "INCH") bad();
            continue;
        }
        if (key == "XSIZE") { if (!(iss >> font.xsize)) bad(); continue; }
        if (key == "YSIZE") { if (!(iss >> font.ysize)) bad(); continue; }
        if (key == "OFFSET") { if (!(iss >> font.offset)) bad(); continue; }
        if (key == "CHAR") {
            auto at = raw.find(rec);
            cur = (at != std::string::npos && raw.size() > at + 5) ? raw[at + 5] : 0;
            inChar = cur != 0;
            inPolygon = false;
            if (inChar) font.glyphs[cur];  // a character with no LINE is still in the font
            else bad();
            continue;
        }
        if (key == "ECHAR") { inChar = false; inPolygon = false; continue; }
        if (!inChar) { bad(); continue; }
        if (key == "LINE") {
            inPolygon = false;
            OdbFontStroke s;
            std::string pol, shape;
            if (!(iss >> s.x1 >> s.y1 >> s.x2 >> s.y2)) { bad(); continue; }
            if (iss >> pol) s.positive = toUpper(pol) != "N";
            if (iss >> shape) s.round = toUpper(shape) != "S";
            if (!(iss >> s.width)) s.width = 0;
            font.glyphs[cur].push_back(s);
            continue;
        }
        // A filled glyph: POLYGON [n] [x y ...], then its points, one or more pairs a line,
        // up to the next LINE / POLYGON / ECHAR
        if (key == "POLYGON") {
            inPolygon = true;
            font.polygons[cur].emplace_back();
            std::vector<double> nums;
            if (!readNumbers(line, 1, nums)) { bad(); continue; }
            size_t from = nums.size() % 2;  // an odd count: the first is the number of points
            for (size_t i = from; i + 1 < nums.size(); i += 2) font.polygons[cur].back().push_back({nums[i], nums[i + 1]});
            continue;
        }
        if (inPolygon) {
            std::vector<double> nums;
            if (!readNumbers(line, 0, nums) || nums.empty() || nums.size() % 2) { bad(); continue; }
            for (size_t i = 0; i + 1 < nums.size(); i += 2) font.polygons[cur].back().push_back({nums[i], nums[i + 1]});
            continue;
        }
        bad();
    }
    return font.xsize > 0 && font.ysize > 0;
}

std::vector<TextStroke> textStrokes(const OdbText& text, const OdbFont& font, std::string* missing,
                                    std::vector<TextPolygon>* polygons) {
    std::vector<TextStroke> out;
    if (font.xsize <= 0 || font.ysize <= 0) return out;
    const double sx = text.xsize / font.xsize, sy = text.ysize / font.ysize;
    const double fontToMm = toUpper(trim(font.units)) == "MM" ? 1.0 : 25.4;  // a font file is in inches
    const double rad = text.angle * 3.14159265358979323846 / 180.0, ca = std::cos(rad), sa = std::sin(rad);
    auto place = [&](double lx, double ly, double& x, double& y) {
        if (text.mirror) lx = -lx;
        x = text.x + lx * ca + ly * sa;   // clockwise, y up
        y = text.y - lx * sa + ly * ca;
    };
    const bool textPositive = text.polarity != "N";
    for (size_t i = 0; i < text.text.size(); i++) {
        char c = text.text[i];
        auto it = font.glyphs.find(c);
        if (it == font.glyphs.end()) {
            if (missing && c != ' ' && missing->find(c) == std::string::npos) missing->push_back(c);
            continue;
        }
        // the glyph's own zero sits on the text's location; one box on per character
        const double cellX = text.vertical ? 0.0 : (double)i * text.xsize;
        const double cellY = text.vertical ? (double)i * text.ysize : 0.0;
        for (auto& s : it->second) {
            TextStroke t;
            place(cellX + s.x1 * sx, cellY + s.y1 * sy, t.x1, t.y1);
            place(cellX + s.x2 * sx, cellY + s.y2 * sy, t.x2, t.y2);
            t.width = s.width * fontToMm * (text.weight > 0 ? text.weight : 1.0);
            t.positive = textPositive == s.positive;
            t.round = s.round;
            out.push_back(t);
        }
        auto pit = font.polygons.find(c);
        if (polygons && pit != font.polygons.end()) {
            for (auto& ring : pit->second) {
                if (ring.size() < 3) continue;
                TextPolygon poly;
                poly.positive = textPositive;
                for (auto& [gx, gy] : ring) {
                    double x, y;
                    place(cellX + gx * sx, cellY + gy * sy, x, y);
                    poly.points.push_back({x, y});
                }
                polygons->push_back(std::move(poly));
            }
        }
    }
    return out;
}

} // namespace odb2kicad
