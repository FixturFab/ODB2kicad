#include "odb_parser/symbols.h"
#include <algorithm>
#include <cmath>
#include <regex>
#include <set>

namespace odb2kicad {

namespace {

using Ring = std::vector<std::pair<double,double>>;
const double kPi = 3.14159265358979323846;

// points on a circle: a full turn of radius r keeps its chord error near 0.1% of r (64 points)
int arcSteps(double sweep) { return std::max(2, (int)std::ceil(std::abs(sweep) / (2 * kPi) * 64)); }

Ring circleRing(double r) {
    Ring out;
    for (int i = 0; i < 64; i++) out.push_back({r * std::cos(2 * kPi * i / 64), r * std::sin(2 * kPi * i / 64)});
    return out;
}

Ring rectRing(double w, double h) { return {{-w / 2, -h / 2}, {w / 2, -h / 2}, {w / 2, h / 2}, {-w / 2, h / 2}}; }

// Rectangle with rounded (or chamfered) corners of size r. ODB++ numbers the corners
// counter-clockwise from the top right: 1 top right, 2 top left, 3 bottom left, 4 bottom right.
Ring cornerRect(double w, double h, double r, bool chamfer, const std::set<int>& corners) {
    r = std::max(0.0, std::min({r, w / 2, h / 2}));
    if (r <= 0) return rectRing(w, h);
    Ring out;
    const int sx[4] = {1, -1, -1, 1}, sy[4] = {1, 1, -1, -1};
    for (int k = 0; k < 4; k++) {
        double a0 = kPi / 2 * k;  // the corner's arc runs a0 .. a0 + 90 deg
        double cx = sx[k] * (w / 2 - r), cy = sy[k] * (h / 2 - r);
        if (!corners.count(k + 1)) {
            out.push_back({sx[k] * w / 2, sy[k] * h / 2});
        } else if (chamfer) {
            out.push_back({cx + r * std::cos(a0), cy + r * std::sin(a0)});
            out.push_back({cx + r * std::cos(a0 + kPi / 2), cy + r * std::sin(a0 + kPi / 2)});
        } else {
            int n = arcSteps(kPi / 2);
            for (int i = 0; i <= n; i++) {
                double a = a0 + kPi / 2 * i / n;
                out.push_back({cx + r * std::cos(a), cy + r * std::sin(a)});
            }
        }
    }
    return out;
}

Ring ovalRing(double w, double h) {
    double r = std::min(w, h) / 2;
    return cornerRect(w, h, r, false, {1, 2, 3, 4});
}

Ring ellipseRing(double w, double h) {
    Ring out;
    for (int i = 0; i < 64; i++) out.push_back({w / 2 * std::cos(2 * kPi * i / 64), h / 2 * std::sin(2 * kPi * i / 64)});
    return out;
}

std::set<int> cornerSet(const std::string& digits) {
    std::set<int> s;
    for (char c : digits)
        if (c >= '1' && c <= '4') s.insert(c - '0');
    if (s.empty()) s = {1, 2, 3, 4};
    return s;
}

// A thermal: the ring (rings, even-odd) with `num` straight air gaps `gap` wide, the first at
// `angle` degrees counter-clockwise from +x, the others evenly spaced. `outer` and `inner` (convex)
// are the ring's two edges. Each piece between two gaps is the wedge between them clipped out of
// both edges, kept as an outer ring and its hole, so the pieces stay separate polygons.
std::vector<Ring> thermal(const Ring& outer, const Ring& inner, double angle, int num, double gap) {
    std::vector<Ring> out;
    if (num <= 0) { out.push_back(outer); out.push_back(inner); return out; }
    for (int k = 0; k < num; k++) {
        double a0 = (angle + 360.0 * k / num) * kPi / 180, a1 = (angle + 360.0 * (k + 1) / num) * kPi / 180;
        double h = gap / 2;
        // half-plane clip helpers: keep points on the left of the directed line through p along d
        auto clip = [](const Ring& in, double px, double py, double dx, double dy) {
            Ring o;
            size_t n = in.size();
            for (size_t i = 0; i < n; i++) {
                auto [ax, ay] = in[i];
                auto [bx, by] = in[(i + 1) % n];
                double sa = dx * (ay - py) - dy * (ax - px), sb = dx * (by - py) - dy * (bx - px);
                if (sa >= 0) o.push_back({ax, ay});
                if ((sa >= 0) != (sb >= 0)) {
                    double t = sa / (sa - sb);
                    o.push_back({ax + t * (bx - ax), ay + t * (by - ay)});
                }
            }
            return o;
        };
        // a sector piece = (outer region minus inner region) inside the wedge; outer and inner are
        // convex (circles, squares, rects), so clipping both by the wedge's two half-planes and
        // keeping them as an outer ring and a hole ring stays exact under even-odd -- when the
        // wedge is under 180 deg. Larger wedges (1 or 2 spokes) are split in two.
        double sweep = a1 - a0;
        int parts = sweep > kPi * 0.99 ? (int)std::ceil(sweep / (kPi * 0.9)) : 1;
        for (int p = 0; p < parts; p++) {
            double b0 = a0 + sweep * p / parts, b1 = a0 + sweep * (p + 1) / parts;
            // start edge: bar along b0 shifted left by h (only for the first part), end edge likewise
            double s0 = p == 0 ? h : 0, s1 = p == parts - 1 ? h : 0;
            double d0x = std::cos(b0), d0y = std::sin(b0), d1x = std::cos(b1), d1y = std::sin(b1);
            auto cut = [&](const Ring& r) {
                // left of the line along b0, offset by s0 to its left
                Ring c = clip(r, -d0y * s0, d0x * s0, d0x, d0y);
                // right of the line along b1, offset by s1 to its right: left of the reversed line
                c = clip(c, d1y * s1, -d1x * s1, -d1x, -d1y);
                return c;
            };
            Ring o = cut(outer), i = cut(inner);
            if (o.size() < 3) continue;
            // the piece is o minus i: as a ring with a hole (even-odd) when i is inside o
            out.push_back(o);
            if (i.size() >= 3) out.push_back(i);
        }
    }
    return out;
}

}  // namespace

// Outline of every standard symbol decodeSymbol has no KiCad shape for (ODB++ 8.1 standard
// symbols), numbers / 1000 like the other sizes. Empty when the name is not one of them.
static std::vector<Ring> standardOutline(const std::string& name) {
    static const std::string N = R"((\d+(?:\.\d+)?))";
    auto num = [](const std::ssub_match& m) { return std::stod(m.str()) / 1000.0; };
    std::smatch m;
    // rect with rounded / chamfered corners, all or some: rect<w>x<h>x(r|c)<rad>[x<corners>]
    if (std::regex_match(name, m, std::regex("rect" + N + "x" + N + "x([rc])" + N + "(?:x([1-4]+))?")))
        return {cornerRect(num(m[1]), num(m[2]), num(m[4]), m[3] == "c", cornerSet(m[5]))};
    if (std::regex_match(name, m, std::regex("oval_h" + N + "x" + N)))
        return {ovalRing(num(m[1]), num(m[2]))};
    if (std::regex_match(name, m, std::regex("el" + N + "x" + N)))
        return {ellipseRing(num(m[1]), num(m[2]))};
    if (std::regex_match(name, m, std::regex("di" + N + "x" + N))) {
        double w = num(m[1]), h = num(m[2]);
        return {{{w / 2, 0}, {0, h / 2}, {-w / 2, 0}, {0, -h / 2}}};
    }
    if (std::regex_match(name, m, std::regex("oct" + N + "x" + N + "x" + N)))
        return {cornerRect(num(m[1]), num(m[2]), num(m[3]), true, {1, 2, 3, 4})};
    if (std::regex_match(name, m, std::regex("hex_l" + N + "x" + N + "x" + N))) {
        double w = num(m[1]), h = num(m[2]), r = num(m[3]);
        return {{{w / 2, 0}, {w / 2 - r, h / 2}, {-w / 2 + r, h / 2}, {-w / 2, 0}, {-w / 2 + r, -h / 2}, {w / 2 - r, -h / 2}}};
    }
    if (std::regex_match(name, m, std::regex("hex_s" + N + "x" + N + "x" + N))) {
        double w = num(m[1]), h = num(m[2]), r = num(m[3]);
        return {{{0, h / 2}, {-w / 2, h / 2 - r}, {-w / 2, -h / 2 + r}, {0, -h / 2}, {w / 2, -h / 2 + r}, {w / 2, h / 2 - r}}};
    }
    if (std::regex_match(name, m, std::regex("tri" + N + "x" + N))) {
        double b = num(m[1]), h = num(m[2]);
        return {{{-b / 2, -h / 2}, {b / 2, -h / 2}, {0, h / 2}}};
    }
    if (std::regex_match(name, m, std::regex("bf([rs])" + N))) {  // butterfly: quadrants 1 and 3
        double r = num(m[2]) / 2;
        Ring q{{0, 0}};
        if (m[1] == "r") {
            int n = arcSteps(kPi / 2);
            for (int i = 0; i <= n; i++) q.push_back({r * std::cos(kPi / 2 * i / n), r * std::sin(kPi / 2 * i / n)});
        } else {
            q.insert(q.end(), {{r, 0}, {r, r}, {0, r}});
        }
        Ring q3;
        for (auto& [x, y] : q) q3.push_back({-x, -y});
        return {q, q3};
    }
    if (std::regex_match(name, m, std::regex("donut_s" + N + "x" + N + "(?:x([rc])" + N + "(?:x([1-4]+))?)?"))) {
        double od = num(m[1]), id = num(m[2]);
        if (!m[3].matched) return {rectRing(od, od), rectRing(id, id)};
        double r = num(m[4]);
        auto cs = cornerSet(m[5]);
        return {cornerRect(od, od, r, m[3] == "c", cs), cornerRect(id, id, std::max(0.0, r - (od - id) / 2), m[3] == "c", cs)};
    }
    if (std::regex_match(name, m, std::regex("donut_rc" + N + "x" + N + "x" + N + "x([rc])" + N + "x([1-4]+)"))) {
        double w = num(m[1]), h = num(m[2]), lw = num(m[3]), r = num(m[5]);
        auto cs = cornerSet(m[6]);
        return {cornerRect(w, h, r, m[4] == "c", cs), cornerRect(w - 2 * lw, h - 2 * lw, std::max(0.0, r - lw), m[4] == "c", cs)};
    }
    if (std::regex_match(name, m, std::regex("donut_rc" + N + "x" + N + "x" + N + "xc" + N))) {
        double w = num(m[1]), h = num(m[2]), lw = num(m[3]), r = num(m[4]);
        return {cornerRect(w, h, r, true, {1, 2, 3, 4}), cornerRect(w - 2 * lw, h - 2 * lw, std::max(0.0, r - lw), true, {1, 2, 3, 4})};
    }
    if (std::regex_match(name, m, std::regex("donut_rc" + N + "x" + N + "x" + N))) {
        double w = num(m[1]), h = num(m[2]), lw = num(m[3]);
        return {rectRing(w, h), rectRing(w - 2 * lw, h - 2 * lw)};
    }
    if (std::regex_match(name, m, std::regex("donut_sr" + N + "x" + N))) {  // square outside, round inside
        double od = num(m[1]), id = num(m[2]);
        return {rectRing(od, od), circleRing(id / 2)};
    }
    if (std::regex_match(name, m, std::regex("donut_o" + N + "x" + N + "x" + N))) {
        double w = num(m[1]), h = num(m[2]), lw = num(m[3]);
        return {ovalRing(w, h), ovalRing(w - 2 * lw, h - 2 * lw)};
    }
    // thermals: <outer>x<inner>x<angle>x<spokes>x<gap>; angle and spoke count are not sizes
    if (std::regex_match(name, m, std::regex("(thr|ths|s_ths|s_tho|s_thr|sr_ths)" + N + "x" + N + "x" + N + "x" + N + "x" + N))) {
        std::string k = m[1];
        double od = num(m[2]), id = num(m[3]), ang = std::stod(m[4].str()), gap = num(m[6]);
        int spokes = (int)std::lround(std::stod(m[5].str()));
        Ring outer = (k == "thr" || k == "ths") ? circleRing(od / 2) : rectRing(od, od);
        Ring inner = (k == "thr" || k == "ths" || k == "sr_ths") ? circleRing(id / 2) : rectRing(id, id);
        return thermal(outer, inner, ang, spokes, gap);
    }
    if (std::regex_match(name, m, std::regex("rc_th([so])" + N + "x" + N + "x" + N + "x" + N + "x" + N + "x" + N))) {
        double w = num(m[2]), h = num(m[3]), ang = std::stod(m[4].str()), gap = num(m[6]), air = num(m[7]);
        int spokes = (int)std::lround(std::stod(m[5].str()));
        return thermal(rectRing(w, h), rectRing(w - 2 * air, h - 2 * air), ang, spokes, gap);
    }
    if (std::regex_match(name, m, std::regex("hole" + N + "(?:x.*)?")))
        return {circleRing(num(m[1]) / 2)};
    return {};
}

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

    // Donut round: donut_r<outer>x<inner>
    std::regex donutRoundRe(R"(donut_r(\d+\.?\d*)x(\d+\.?\d*))");
    if (std::regex_match(name, m, donutRoundRe)) {
        sym.shape = OdbSymbol::ROUND;
        sym.diameter = std::stod(m[1]) / 1000.0;
        sym.width = sym.diameter;
        sym.height = sym.diameter;
        sym.lineWidth = (sym.diameter - std::stod(m[2]) / 1000.0) / 2.0;
        sym.donut = true;
        return sym;
    }

    // Donut round-cornered: donut_rc<w>x<h>x<lw>xr<r>
    std::regex donutRcRe(R"(donut_rc(\d+\.?\d*)x(\d+\.?\d*)x(\d+\.?\d*)xr(\d+\.?\d*))");
    if (std::regex_match(name, m, donutRcRe)) {
        sym.shape = OdbSymbol::ROUNDRECT;
        sym.width = std::stod(m[1]) / 1000.0;
        sym.height = std::stod(m[2]) / 1000.0;
        sym.lineWidth = std::stod(m[3]) / 1000.0;
        sym.cornerRadius = std::stod(m[4]) / 1000.0;
        sym.donut = true;
        return sym;
    }

    sym.outline = standardOutline(name);
    for (auto& ring : sym.outline)  // its extent, so size comparisons and margins see it
        for (auto& [x, y] : ring) {
            sym.width = std::max(sym.width, 2 * std::abs(x));
            sym.height = std::max(sym.height, 2 * std::abs(y));
        }
    return sym;
}

} // namespace odb2kicad
