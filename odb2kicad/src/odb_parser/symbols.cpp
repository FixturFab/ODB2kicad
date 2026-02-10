#include "odb_parser/symbols.h"
#include <regex>

namespace odb2kicad {

OdbSymbol decodeSymbol(const std::string& name) {
    OdbSymbol sym;
    sym.name = name;

    // Rounded rect: rect<w>x<h>xr<r>
    std::regex rrectRe(R"(rect(\d+\.?\d*)x(\d+\.?\d*)xr(\d+\.?\d*))");
    std::smatch m;
    if (std::regex_match(name, m, rrectRe)) {
        sym.shape = OdbSymbol::ROUNDRECT;
        sym.width = std::stod(m[1]) / 1000.0;
        sym.height = std::stod(m[2]) / 1000.0;
        sym.cornerRadius = std::stod(m[3]) / 1000.0;
        return sym;
    }

    // Plain rect: rect<w>x<h>
    std::regex rectRe(R"(rect(\d+\.?\d*)x(\d+\.?\d*))");
    if (std::regex_match(name, m, rectRe)) {
        sym.shape = OdbSymbol::RECT;
        sym.width = std::stod(m[1]) / 1000.0;
        sym.height = std::stod(m[2]) / 1000.0;
        return sym;
    }

    // Oval: oval<w>x<h>
    std::regex ovalRe(R"(oval(\d+\.?\d*)x(\d+\.?\d*))");
    if (std::regex_match(name, m, ovalRe)) {
        sym.shape = OdbSymbol::OVAL;
        sym.width = std::stod(m[1]) / 1000.0;
        sym.height = std::stod(m[2]) / 1000.0;
        return sym;
    }

    // Round: r<d>
    std::regex roundRe(R"(r(\d+\.?\d*))");
    if (std::regex_match(name, m, roundRe)) {
        sym.shape = OdbSymbol::ROUND;
        sym.diameter = std::stod(m[1]) / 1000.0;
        sym.width = sym.diameter;
        sym.height = sym.diameter;
        return sym;
    }

    // Square: s<d>
    std::regex sqRe(R"(s(\d+\.?\d*))");
    if (std::regex_match(name, m, sqRe)) {
        sym.shape = OdbSymbol::SQUARE;
        double side = std::stod(m[1]) / 1000.0;
        sym.width = side;
        sym.height = side;
        return sym;
    }

    return sym;
}

} // namespace odb2kicad
