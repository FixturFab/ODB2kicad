#pragma once
#include "model/odb_design.h"
#include <string>

namespace odb2kicad {

// Flattening tolerance (largest chord-to-arc gap) for surface arcs, in file units:
// 0.1 mil for INCH (or no UNITS line, the spec's default), 2.5 um for MM.
double contourTolerance(const std::string& units);

// Append an arc from the contour's last point to (xe, ye) about (xc, yc): intermediate points
// flagged interp, then the end point flagged arc. Start == end is a full circle.
void appendContourArc(OdbContour& c, double xe, double ye, double xc, double yc, bool cw, double tol);

// One OB / OS / OC / OE record of a surface; false when the line is none of them.
bool parseContourRecord(const std::string& line, OdbSurface* surface, OdbContour*& contour, double tol);

} // namespace odb2kicad
