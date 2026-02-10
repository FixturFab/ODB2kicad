#pragma once
#include <cmath>

namespace odb2kicad {

// ODB++ to KiCad coordinate transform: negate Y
inline double toKicadY(double odbY) { return -odbY; }
inline double toKicadX(double odbX) { return odbX; }

constexpr double PI = 3.14159265358979323846;

inline double degToRad(double deg) { return deg * PI / 180.0; }

} // namespace odb2kicad
