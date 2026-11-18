#pragma once
#include "model/odb_design.h"
#include <string>
#include <utility>
#include <vector>

namespace odb2kicad {

// A vector font (fonts/<name>): an optional UNITS line (the font's own units, INCH by default),
// XSIZE / YSIZE / OFFSET, then CHAR <c> ... ECHAR blocks of LINE strokes (R round, S square
// ends) and POLYGON filled outlines. Lines that cannot be read are skipped and their numbers
// appended to *badLines ("error while reading font file - <name>, line - <n>").
bool parseFont(const std::string& path, OdbFont& font, std::vector<int>* badLines = nullptr);

// One stroke of a drawn text, in the text's layer coordinates and units
struct TextStroke {
    double x1 = 0, y1 = 0, x2 = 0, y2 = 0;
    double width = 0;       // mm
    bool positive = true;   // false: the stroke clears (a negative text, or a negative font line)
    bool round = true;      // the font line's ends: R round, S square
};

// One filled outline of a drawn text (a POLYGON glyph), in the text's layer coordinates
struct TextPolygon {
    std::vector<std::pair<double, double>> points;
    bool positive = true;
};

// The strokes of a T / V record drawn with a font. Glyph coordinates are in the font's units
// with the glyph's own zero at its box's corner: each character is scaled by xsize / XSIZE and
// ysize / YSIZE and set one box (xsize) on along the text (V: one ysize on, upwards), mirrored
// in X when the record says so, turned clockwise by its angle, the first glyph's zero on the
// record's location. A stroke is `weight` font line widths wide (the standard font's are
// 12 mil). Characters the font does not have (a blank among them) take their box and draw
// nothing; they are appended to *missing when given. Filled glyphs go to *polygons.
std::vector<TextStroke> textStrokes(const OdbText& text, const OdbFont& font, std::string* missing = nullptr,
                                    std::vector<TextPolygon>* polygons = nullptr);

} // namespace odb2kicad
