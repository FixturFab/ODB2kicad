#include "odb_parser/contour.h"
#include "util/string_utils.h"
#include <algorithm>
#include <cmath>
#include <sstream>

namespace odb2kicad {

double contourTolerance(const std::string& units) {
    std::string u = toUpper(trim(units));
    return (u == "MM") ? 0.0025 : 0.0001;
}

void appendContourArc(OdbContour& c, double xe, double ye, double xc, double yc, bool cw, double tol) {
    if (c.points.empty()) {
        c.points.push_back({xe, ye});
        return;
    }
    const double kPi = 3.14159265358979323846;
    OdbPoint s = c.points.back();
    double r0 = std::hypot(s.x - xc, s.y - yc), r1 = std::hypot(xe - xc, ye - yc);
    double a0 = std::atan2(s.y - yc, s.x - xc), a1 = std::atan2(ye - yc, xe - xc);
    double sweep = cw ? a0 - a1 : a1 - a0;
    while (sweep <= 1e-12) sweep += 2 * kPi;   // start == end: a full circle
    double r = std::max(r0, r1);
    double step = (r > tol) ? 2 * std::acos(1 - tol / r) : sweep;
    int n = std::clamp((int)std::ceil(sweep / step), 1, 720);
    for (int k = 1; k < n; k++) {
        double t = (double)k / n;
        double a = a0 + (cw ? -sweep : sweep) * t;
        double rr = r0 + (r1 - r0) * t;
        OdbPoint p;
        p.x = xc + rr * std::cos(a);
        p.y = yc + rr * std::sin(a);
        p.interp = true;
        c.points.push_back(p);
    }
    OdbPoint e;
    e.x = xe; e.y = ye;
    e.arc = true; e.cw = cw; e.xc = xc; e.yc = yc;
    c.points.push_back(e);
}

bool parseContourRecord(const std::string& line, OdbSurface* surface, OdbContour*& contour, double tol) {
    if (startsWith(line, "OB ")) {
        if (!surface) return true;
        surface->contours.emplace_back();
        contour = &surface->contours.back();
        std::istringstream iss(line.substr(3));
        double x = 0, y = 0;
        std::string dir;
        iss >> x >> y >> dir;
        contour->isHole = (dir == "H");
        contour->points.push_back({x, y});
        return true;
    }
    if (startsWith(line, "OS ")) {
        if (!contour) return true;
        std::istringstream iss(line.substr(3));
        double x = 0, y = 0;
        iss >> x >> y;
        contour->points.push_back({x, y});
        return true;
    }
    if (startsWith(line, "OC ")) {
        // OC xe ye xc yc cw
        if (!contour) return true;
        std::istringstream iss(line.substr(3));
        double xe = 0, ye = 0, xc = 0, yc = 0;
        std::string cw;
        iss >> xe >> ye >> xc >> yc >> cw;
        appendContourArc(*contour, xe, ye, xc, yc, toUpper(cw) == "Y", tol);
        return true;
    }
    if (line == "OE") {
        contour = nullptr;
        return true;
    }
    return false;
}

} // namespace odb2kicad
