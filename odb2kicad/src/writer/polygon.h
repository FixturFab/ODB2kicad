#pragma once
// Small polygon toolkit for the writer: a boolean difference for negative-polarity
// features, and outlines of flashed / stroked shapes. No external dependencies.
#include <utility>
#include <vector>

namespace odb2kicad {
namespace poly {

using Pt = std::pair<double, double>;
using Ring = std::vector<Pt>;           // closed implicitly (last vertex is not the first repeated)
using Shape = std::vector<Ring>;        // rings filled even-odd (an island and its holes, a donut)

struct Island {
    Ring outer;                         // positive signed area
    std::vector<Ring> holes;            // negative signed area
};

double signedArea(const Ring& r);

// Region of `a` (the union of its shapes) minus the union of the shapes in `b`, as islands with
// their holes. Coordinates are snapped to a 1 nm grid (inputs in mm); edges are split where they
// cross and kept when the result differs on their two sides, so shapes may overlap, touch and
// share edges.
std::vector<Island> difference(const std::vector<Shape>& a, const std::vector<Shape>& b);

// True when the bounding boxes of a and b overlap
bool boxesOverlap(const Shape& a, const Shape& b);

// Points per full turn so the chord error stays under 5 um (min 16)
int circleSegments(double r);

Ring circle(double cx, double cy, double r);
// A stroke of width w from (x1, y1) to (x2, y2): round ends, or square ends (ODB++ "s" symbol)
Ring stroke(double x1, double y1, double x2, double y2, double w, bool squareEnds);
// An arc stroke: from start to end about the centre, the shorter or longer way as `ccw` says
// (in the coordinates given), round or square ends. A full circle when start == end: a donut.
Shape arcStroke(double xs, double ys, double xe, double ye, double xc, double yc, bool ccw, double w,
                bool squareEnds);

}  // namespace poly
}  // namespace odb2kicad
