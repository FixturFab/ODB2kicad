#include "odb_parser/symbols.h"
#include "util/string_utils.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <map>
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

// ---- thermals and the other symbols built from more than one even-odd shape -------------------

// What a standard symbol draws: `fill` (rings, even-odd), plus `add` unioned with it, less `cut`.
struct Geom {
    std::vector<Ring> fill;
    std::vector<std::vector<Ring>> add, cut;
};

// A bar `2 * half` wide along the unit direction (dx, dy) from s0 to s1 past the point (px, py)
Ring bar(double px, double py, double dx, double dy, double s0, double s1, double half) {
    double nx = -dy * half, ny = dx * half;
    return {{px + dx * s0 - nx, py + dy * s0 - ny}, {px + dx * s1 - nx, py + dy * s1 - ny},
            {px + dx * s1 + nx, py + dy * s1 + ny}, {px + dx * s0 + nx, py + dy * s0 + ny}};
}

// Spoke k's angle (radians, counter-clockwise from +x): the first at `angle` degrees, the others
// evenly spaced
double spokeAngle(double angle, int num, int k) { return (angle + 360.0 * k / num) * kPi / 180; }

// True when the spoke angle (degrees) points at a corner: 45 + a multiple of 90
bool diagonal(double deg) {
    double m = std::fmod(std::fmod(deg, 90.0) + 90.0, 90.0);
    return std::abs(m - 45.0) < 1e-6;
}

// Straight air gaps `gap` wide from the centre out along each spoke
void centreGaps(Geom& g, double angle, int num, double gap, double reach) {
    for (int k = 0; k < num; k++) {
        double a = spokeAngle(angle, num, k);
        g.cut.push_back({bar(0, 0, std::cos(a), std::sin(a), 0, reach, gap / 2)});
    }
}

// A closed centre line, counter-clockwise: the point and its outward unit normal at t in [0, 1)
struct PathPt { double x, y, nx, ny; };
using PathFn = std::function<PathPt(double)>;

PathFn circlePath(double r) {
    return [r](double t) {
        double a = 2 * kPi * t;
        return PathPt{r * std::cos(a), r * std::sin(a), std::cos(a), std::sin(a)};
    };
}

// a square of half side c, from its bottom right corner up
PathFn squarePath(double c) {
    return [c](double t) {
        double s = (t - std::floor(t)) * 8 * c;
        int k = std::min(3, (int)(s / (2 * c)));
        double u = s - k * 2 * c;
        switch (k) {
            case 0: return PathPt{c, -c + u, 1, 0};
            case 1: return PathPt{c - u, c, 0, 1};
            case 2: return PathPt{-c, c - u, -1, 0};
            default: return PathPt{-c + u, -c, 0, -1};
        }
    };
}

// a stadium (oval) of half sizes a x b, its ends round
PathFn stadiumPath(double a, double b) {
    bool tall = b > a;
    if (tall) std::swap(a, b);
    double rho = b, e = a - b, P = 2 * kPi * rho + 4 * e;
    return [=](double t) {
        double s = (t - std::floor(t)) * P;
        PathPt q;
        if (s < kPi * rho) {  // right end
            double ang = -kPi / 2 + s / rho;
            q = {e + rho * std::cos(ang), rho * std::sin(ang), std::cos(ang), std::sin(ang)};
        } else if ((s -= kPi * rho) < 2 * e) {  // top
            q = {e - s, rho, 0, 1};
        } else if ((s -= 2 * e) < kPi * rho) {  // left end
            double ang = kPi / 2 + s / rho;
            q = {-e + rho * std::cos(ang), rho * std::sin(ang), std::cos(ang), std::sin(ang)};
        } else {  // bottom
            s -= kPi * rho;
            q = {-e + s, -rho, 0, -1};
        }
        if (tall) q = {-q.y, q.x, -q.ny, q.nx};  // turned a quarter, still counter-clockwise
        return q;
    };
}

// A thermal whose copper ends are round: the band `lw` wide along the closed centre line, broken
// wherever the centre line comes within `gap / 2 + lw / 2` of a spoke (a ray from the centre), each
// piece ending in a half circle so its tip stays about `gap / 2` from the spoke. Without spokes,
// the whole band.
Geom roundEndThermal(const PathFn& path, double lw, double angle, int num, double gap) {
    Geom g;
    const int N = 256;
    double h = lw / 2;
    if (num <= 0) {
        Ring o, i;
        for (int k = 0; k < N; k++) {
            PathPt p = path((double)k / N);
            o.push_back({p.x + p.nx * h, p.y + p.ny * h});
            i.push_back({p.x - p.nx * h, p.y - p.ny * h});
        }
        g.fill = {o, i};
        return g;
    }
    std::vector<std::pair<double,double>> rays;
    for (int k = 0; k < num; k++) rays.push_back({std::cos(spokeAngle(angle, num, k)), std::sin(spokeAngle(angle, num, k))});
    double reach = gap / 2 + h;
    auto covered = [&](double t) {
        PathPt p = path(t);
        for (auto [dx, dy] : rays)
            if (p.x * dx + p.y * dy >= 0 && std::abs(dx * p.y - dy * p.x) < reach) return true;
        return false;
    };
    std::vector<char> in(N);
    for (int k = 0; k < N; k++) in[k] = covered((double)k / N);
    // the boundary between an inside sample ti and the outside sample to, closer to to's side
    auto edge = [&](double ti, double to) {
        for (int it = 0; it < 48; it++) {
            double m = (ti + to) / 2;
            (covered(m) ? ti : to) = m;
        }
        return to;
    };
    for (int a = 0; a < N; a++) {
        if (in[a] || !in[(a + N - 1) % N]) continue;  // a run of uncut samples starts at a
        int b = a;
        while (!in[(b + 1) % N]) b++;
        double ts = edge((a - 1.0) / N, (double)a / N), te = edge((b + 1.0) / N, (double)b / N);
        std::vector<PathPt> line{path(ts)};
        for (int k = a; k <= b; k++) line.push_back(path((double)k / N));
        line.push_back(path(te));
        Ring r;
        for (auto& p : line) r.push_back({p.x + p.nx * h, p.y + p.ny * h});
        auto cap = [&](const PathPt& c, double sx) {  // half circle from +sx*n, a half turn counter-clockwise
            double a0 = std::atan2(sx * c.ny, sx * c.nx);
            int n = arcSteps(kPi);
            for (int i = 1; i < n; i++) r.push_back({c.x + h * std::cos(a0 + kPi * i / n), c.y + h * std::sin(a0 + kPi * i / n)});
        };
        cap(line.back(), 1);
        for (size_t k = line.size(); k-- > 0;) r.push_back({line[k].x - line[k].nx * h, line[k].y - line[k].ny * h});
        cap(line.front(), -1);
        g.add.push_back({r});
    }
    return g;
}

// Round each corner i of a counter-clockwise polygon by rad[i] (0: left sharp): an arc tangent to
// both edges, its tangent length held to half the shorter edge.
Ring fillet(const Ring& pts, const std::vector<double>& rad) {
    Ring out;
    size_t n = pts.size();
    for (size_t i = 0; i < n; i++) {
        auto [bx, by] = pts[i];
        auto [ax, ay] = pts[(i + n - 1) % n];
        auto [cx, cy] = pts[(i + 1) % n];
        double l1 = std::hypot(ax - bx, ay - by), l2 = std::hypot(cx - bx, cy - by);
        if (rad[i] <= 0 || l1 < 1e-12 || l2 < 1e-12) { out.push_back(pts[i]); continue; }
        double u1x = (ax - bx) / l1, u1y = (ay - by) / l1, u2x = (cx - bx) / l2, u2y = (cy - by) / l2;
        double th = std::acos(std::max(-1.0, std::min(1.0, u1x * u2x + u1y * u2y)));
        if (th < 1e-9 || th > kPi - 1e-9) { out.push_back(pts[i]); continue; }
        double t = std::min(rad[i] / std::tan(th / 2), std::min(l1, l2) / 2);
        double r = t * std::tan(th / 2);
        double bisx = u1x + u2x, bisy = u1y + u2y, bl = std::hypot(bisx, bisy);
        double ccx = bx + bisx / bl * r / std::sin(th / 2), ccy = by + bisy / bl * r / std::sin(th / 2);
        double p1x = bx + u1x * t, p1y = by + u1y * t, p2x = bx + u2x * t, p2y = by + u2y * t;
        double a1 = std::atan2(p1y - ccy, p1x - ccx), a2 = std::atan2(p2y - ccy, p2x - ccx);
        double sw = a2 - a1;
        while (sw > kPi) sw -= 2 * kPi;
        while (sw < -kPi) sw += 2 * kPi;
        int steps = arcSteps(sw);
        for (int k = 0; k <= steps; k++) out.push_back({ccx + r * std::cos(a1 + sw * k / steps), ccy + r * std::sin(a1 + sw * k / steps)});
    }
    return out;
}

// Corner radii for a counter-clockwise polygon: `ra` where the inside angle is 90 deg or less,
// `ro` where it is more (obtuse and reflex corners)
std::vector<double> cornerRadii(const Ring& pts, double ra, double ro) {
    std::vector<double> out;
    size_t n = pts.size();
    for (size_t i = 0; i < n; i++) {
        auto [ax, ay] = pts[(i + n - 1) % n];
        auto [bx, by] = pts[i];
        auto [cx, cy] = pts[(i + 1) % n];
        double ux = ax - bx, uy = ay - by, vx = cx - bx, vy = cy - by;
        double th = std::atan2(std::abs(ux * vy - uy * vx), ux * vx + uy * vy);  // between the edges, 0..pi
        bool left = (bx - ax) * (cy - by) - (by - ay) * (cx - bx) > 0;          // a left turn: convex
        double inside = left ? th : 2 * kPi - th;
        out.push_back(inside <= kPi / 2 + 1e-9 ? ra : ro);
    }
    return out;
}

// The quarter of material a concave corner of radius r adds at the corner (kx, ky) between an edge
// running along sx and one running along sy
Ring innerFillet(double kx, double ky, double sx, double sy, double r) {
    Ring out{{kx, ky}};
    double cx = kx + sx * r, cy = ky + sy * r;
    double a0 = std::atan2(-sy, 0.0), a1 = std::atan2(0.0, -sx);
    double sw = a1 - a0;
    while (sw > kPi) sw -= 2 * kPi;
    while (sw < -kPi) sw += 2 * kPi;
    int steps = arcSteps(sw);
    for (int k = 0; k <= steps; k++) out.push_back({cx + r * std::cos(a0 + sw * k / steps), cy + r * std::sin(a0 + sw * k / steps)});
    return out;
}

Ring dropRepeats(const Ring& in) {
    Ring out;
    for (auto& p : in)
        if (out.empty() || std::hypot(p.first - out.back().first, p.second - out.back().second) > 1e-12) out.push_back(p);
    while (out.size() > 1 && std::hypot(out[0].first - out.back().first, out[0].second - out.back().second) < 1e-12) out.pop_back();
    return out;
}

// A line `w` wide from (x0, y0) to (x1, y1) whose whole length, ends included, is that span:
// round ends (half circles inside the span) or square
Ring spanBar(double x0, double y0, double x1, double y1, double w, bool round) {
    double L = std::hypot(x1 - x0, y1 - y0);
    if (L < 1e-12) return circleRing(w / 2);
    double dx = (x1 - x0) / L, dy = (y1 - y0) / L, h = w / 2;
    if (!round || L <= w) {
        if (round) {  // shorter than wide: a circle-ended stub
            Ring c;
            for (auto [x, y] : circleRing(h)) c.push_back({(x0 + x1) / 2 + x, (y0 + y1) / 2 + y});
            return c;
        }
        return bar(x0, y0, dx, dy, 0, L, h);
    }
    double sx = x0 + dx * h, sy = y0 + dy * h, ex = x1 - dx * h, ey = y1 - dy * h, a = std::atan2(dy, dx);
    Ring out;
    int n = arcSteps(kPi);
    for (int i = 0; i <= n; i++) out.push_back({ex + h * std::cos(a - kPi / 2 + kPi * i / n), ey + h * std::sin(a - kPi / 2 + kPi * i / n)});
    for (int i = 0; i <= n; i++) out.push_back({sx + h * std::cos(a + kPi / 2 + kPi * i / n), sy + h * std::sin(a + kPi / 2 + kPi * i / n)});
    return out;
}

}  // namespace

// A pattern compiled once: decodeSymbol runs for every symbol of every layer
static const std::regex& rx(const std::string& pattern) {
    static std::map<std::string, std::regex> cache;
    auto it = cache.find(pattern);
    if (it == cache.end()) it = cache.emplace(pattern, std::regex(pattern)).first;
    return it->second;
}

// Outline of every standard symbol decodeSymbol has no KiCad shape for (ODB++ 8.1 standard
// symbols), numbers / 1000 like the other sizes. Empty when the name is not one of them.
static Geom standardOutline(const std::string& name) {
    static const std::string N = R"((\d+(?:\.\d+)?))";
    // optional rounded / chamfered corners: x(r|c)<rad>[x<corners>]
    static const std::string CORNER = R"((?:x([rc])(\d+(?:\.\d+)?)(?:x([1-4]+))?)?)";
    auto num = [](const std::ssub_match& m) { return std::stod(m.str()) / 1000.0; };
    auto opt = [&](const std::ssub_match& m) { return m.matched ? num(m) : 0.0; };
    auto one = [](std::vector<Ring> fill) { Geom g; g.fill = std::move(fill); return g; };
    std::smatch m;
    // rect with rounded / chamfered corners, all or some: rect<w>x<h>x(r|c)<rad>[x<corners>]
    if (std::regex_match(name, m, rx("rect" + N + "x" + N + "x([rc])" + N + "(?:x([1-4]+))?")))
        return one({cornerRect(num(m[1]), num(m[2]), num(m[4]), m[3] == "c", cornerSet(m[5]))});
    // half oval: square on the left, a half circle on the right
    if (std::regex_match(name, m, rx("oval_h" + N + "x" + N))) {
        double w = num(m[1]), h = num(m[2]), r = h / 2;
        if (w < r) return one({rectRing(w, h)});
        Ring out{{-w / 2, -r}};
        int n = arcSteps(kPi);
        for (int i = 0; i <= n; i++) out.push_back({w / 2 - r + r * std::cos(-kPi / 2 + kPi * i / n), r * std::sin(-kPi / 2 + kPi * i / n)});
        out.push_back({-w / 2, r});
        return one({dropRepeats(out)});
    }
    if (std::regex_match(name, m, rx("el" + N + "x" + N)))
        return one({ellipseRing(num(m[1]), num(m[2]))});
    if (std::regex_match(name, m, rx("di" + N + "x" + N))) {
        double w = num(m[1]), h = num(m[2]);
        return one({{{w / 2, 0}, {0, h / 2}, {-w / 2, 0}, {0, -h / 2}}});
    }
    if (std::regex_match(name, m, rx("oct" + N + "x" + N + "x" + N)))
        return one({cornerRect(num(m[1]), num(m[2]), num(m[3]), true, {1, 2, 3, 4})});
    if (std::regex_match(name, m, rx("hex_l" + N + "x" + N + "x" + N))) {
        double w = num(m[1]), h = num(m[2]), r = num(m[3]);
        return one({{{w / 2, 0}, {w / 2 - r, h / 2}, {-w / 2 + r, h / 2}, {-w / 2, 0}, {-w / 2 + r, -h / 2}, {w / 2 - r, -h / 2}}});
    }
    if (std::regex_match(name, m, rx("hex_s" + N + "x" + N + "x" + N))) {
        double w = num(m[1]), h = num(m[2]), r = num(m[3]);
        return one({{{0, h / 2}, {-w / 2, h / 2 - r}, {-w / 2, -h / 2 + r}, {0, -h / 2}, {w / 2, -h / 2 + r}, {w / 2, h / 2 - r}}});
    }
    if (std::regex_match(name, m, rx("tri" + N + "x" + N))) {
        double b = num(m[1]), h = num(m[2]);
        return one({{{-b / 2, -h / 2}, {b / 2, -h / 2}, {0, h / 2}}});
    }
    if (std::regex_match(name, m, rx("bf([rs])" + N))) {  // butterfly: quadrants 1 and 3
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
        return one({q, q3});
    }
    if (std::regex_match(name, m, rx("donut_s" + N + "x" + N + "(?:x([rc])" + N + "(?:x([1-4]+))?)?"))) {
        double od = num(m[1]), id = num(m[2]);
        if (!m[3].matched) return one({rectRing(od, od), rectRing(id, id)});
        double r = num(m[4]);
        auto cs = cornerSet(m[5]);
        return one({cornerRect(od, od, r, m[3] == "c", cs), cornerRect(id, id, std::max(0.0, r - (od - id) / 2), m[3] == "c", cs)});
    }
    if (std::regex_match(name, m, rx("donut_rc" + N + "x" + N + "x" + N + "x([rc])" + N + "x([1-4]+)"))) {
        double w = num(m[1]), h = num(m[2]), lw = num(m[3]), r = num(m[5]);
        auto cs = cornerSet(m[6]);
        return one({cornerRect(w, h, r, m[4] == "c", cs), cornerRect(w - 2 * lw, h - 2 * lw, std::max(0.0, r - lw), m[4] == "c", cs)});
    }
    if (std::regex_match(name, m, rx("donut_rc" + N + "x" + N + "x" + N + "xc" + N))) {
        double w = num(m[1]), h = num(m[2]), lw = num(m[3]), r = num(m[4]);
        return one({cornerRect(w, h, r, true, {1, 2, 3, 4}), cornerRect(w - 2 * lw, h - 2 * lw, std::max(0.0, r - lw), true, {1, 2, 3, 4})});
    }
    if (std::regex_match(name, m, rx("donut_rc" + N + "x" + N + "x" + N))) {
        double w = num(m[1]), h = num(m[2]), lw = num(m[3]);
        return one({rectRing(w, h), rectRing(w - 2 * lw, h - 2 * lw)});
    }
    if (std::regex_match(name, m, rx("donut_sr" + N + "x" + N))) {  // square outside, round inside
        double od = num(m[1]), id = num(m[2]);
        return one({rectRing(od, od), circleRing(id / 2)});
    }
    if (std::regex_match(name, m, rx("donut_o" + N + "x" + N + "x" + N))) {
        double w = num(m[1]), h = num(m[2]), lw = num(m[3]);
        return one({ovalRing(w, h), ovalRing(w - 2 * lw, h - 2 * lw)});
    }
    // ---- thermals: <outer>x<inner>x<angle>x<spokes>x<gap>...; angle and spoke count are not sizes
    auto spokes = [](const std::ssub_match& s) { return (int)std::lround(std::stod(s.str())); };
    // round, copper ends rounded: pieces of the ring with half-circle ends
    if (std::regex_match(name, m, rx("thr" + N + "x" + N + "x" + N + "x" + N + "x" + N))) {
        double od = num(m[1]), id = num(m[2]);
        return roundEndThermal(circlePath((od + id) / 4), (od - id) / 2, std::stod(m[3].str()), spokes(m[4]), num(m[5]));
    }
    // line thermal: the square ring's sides as lines (os - is) / 2 wide with round ends
    if (std::regex_match(name, m, rx("s_thr" + N + "x" + N + "x" + N + "x" + N + "x" + N))) {
        double os = num(m[1]), is = num(m[2]);
        return roundEndThermal(squarePath((os + is) / 4), (os - is) / 2, std::stod(m[3].str()), spokes(m[4]), num(m[5]));
    }
    // ring less straight gaps: round (ths), square (s_ths, optionally rounded / chamfered corners),
    // square outside and round inside (sr_ths)
    if (std::regex_match(name, m, rx("(ths|s_ths|sr_ths)" + N + "x" + N + "x" + N + "x" + N + "x" + N + CORNER))) {
        std::string k = m[1];
        double od = num(m[2]), id = num(m[3]), gap = num(m[6]);
        Geom g;
        if (k == "ths") {
            g.fill = {circleRing(od / 2), circleRing(id / 2)};
        } else {
            double r = opt(m[8]);
            bool ch = m[7] == "c";
            auto cs = cornerSet(m[9]);
            g.fill = {cornerRect(od, od, r, ch, cs),
                      k == "sr_ths" ? circleRing(id / 2) : cornerRect(id, id, std::max(0.0, r - (od - id) / 2), ch, cs)};
        }
        centreGaps(g, std::stod(m[4].str()), spokes(m[5]), gap, od + gap);
        return g;
    }
    // rectangular thermal: rc_ths<w>x<h>x<angle>x<spokes>x<gap>x<air gap or line width>[corners];
    // a gap at 45 deg (+ 90 n) runs through the rectangle's corner, as the inner and outer corners
    // of an even ring line up at 45 deg. Open corners (s_tho, rc_tho): a corner spoke takes the
    // corner square out, so the sides end `gap` short of the next side's inside edge.
    bool squareOpen = std::regex_match(name, m, rx("s_tho" + N + "x" + N + "x" + N + "x" + N + "x" + N));
    if (squareOpen || std::regex_match(name, m, rx("rc_th([so])" + N + "x" + N + "x" + N + "x" + N + "x" + N + "x" + N + CORNER))) {
        double w, h, ang, gap, air, r = 0;
        int n;
        bool open, ch = false;
        std::set<int> cs{1, 2, 3, 4};
        if (squareOpen) {
            w = h = num(m[1]);
            air = (num(m[1]) - num(m[2])) / 2;
            ang = std::stod(m[3].str()), n = spokes(m[4]), gap = num(m[5]);
            open = true;
        } else {
            w = num(m[2]), h = num(m[3]), ang = std::stod(m[4].str()), n = spokes(m[5]), gap = num(m[6]), air = num(m[7]);
            open = m[1] == "o";
            r = opt(m[9]), ch = m[8] == "c", cs = cornerSet(m[10]);
        }
        Geom g;
        g.fill = {cornerRect(w, h, r, ch, cs), cornerRect(w - 2 * air, h - 2 * air, std::max(0.0, r - air), ch, cs)};
        double reach = w + h + gap;
        for (int k = 0; k < n; k++) {
            double deg = ang + 360.0 * k / n, a = spokeAngle(ang, n, k);
            double dx = std::cos(a), dy = std::sin(a);
            if (!diagonal(deg)) {
                g.cut.push_back({bar(0, 0, dx, dy, 0, reach, gap / 2)});
                continue;
            }
            double sx = dx > 0 ? 1 : -1, sy = dy > 0 ? 1 : -1;
            if (open) {
                double x0 = w / 2 - air - gap, y0 = h / 2 - air - gap;
                g.cut.push_back({{{sx * x0, sy * y0}, {sx * (w / 2 + gap), sy * y0}, {sx * (w / 2 + gap), sy * (h / 2 + gap)}, {sx * x0, sy * (h / 2 + gap)}}});
            } else {
                double back = std::min(gap, std::min(w, h) / 2 - air);
                g.cut.push_back({bar(sx * (w / 2 - air), sy * (h / 2 - air), dx, dy, -std::max(0.0, back), reach, gap / 2)});
            }
        }
        return g;
    }
    // oval thermal: o_ths<ow>x<oh>x<angle>x<spokes>x<gap>x<lw>; oblong: the same with x<r|s>,
    // the copper ends round (r) or straight (s)
    if (std::regex_match(name, m, rx("(o_ths|oblong_ths)" + N + "x" + N + "x" + N + "x" + N + "x" + N + "x" + N + "(?:x([rs]))?"))) {
        double w = num(m[2]), h = num(m[3]), ang = std::stod(m[4].str()), gap = num(m[6]), lw = num(m[7]);
        int n = spokes(m[5]);
        if (m[1] == "oblong_ths" && m[8] == "r")
            return roundEndThermal(stadiumPath((w - lw) / 2, (h - lw) / 2), lw, ang, n, gap);
        Geom g;
        g.fill = {ovalRing(w, h), ovalRing(w - 2 * lw, h - 2 * lw)};
        centreGaps(g, ang, n, gap, w + h + gap);
        return g;
    }
    // moire: rings rw wide, ring i (1..nr) reaching out to i * (rg + rw / 2), and a crosshair of two
    // lines lw wide and ll long at la and la + 90 deg
    if (std::regex_match(name, m, rx("moire" + N + "x" + N + "x" + N + "x" + N + "x" + N + "x" + N))) {
        double rw = num(m[1]), rg = num(m[2]), lw = num(m[4]), ll = num(m[5]), la = std::stod(m[6].str()) * kPi / 180;
        int nr = spokes(m[3]);
        Geom g;
        for (int i = 1; i <= nr; i++) {
            double ro = i * (rg + rw / 2), ri = ro - rw;
            if (ri > 1e-9) g.add.push_back({circleRing(ro), circleRing(ri)});
            else g.add.push_back({circleRing(ro)});
        }
        for (double a : {la, la + kPi / 2})
            g.add.push_back({bar(0, 0, std::cos(a), std::sin(a), -ll / 2, ll / 2, lw / 2)});
        return g;
    }
    // ---- symbols for solder stencils: corner radii ra (inside angle up to 90 deg) and ro (more),
    // written x<ra>x<ro> or xra<ra>xro<ro>, both optional
    static const std::string RA = R"((?:x(?:ra)?(\d+(?:\.\d+)?))?)", RO = R"((?:x(?:ro)?(\d+(?:\.\d+)?))?)";
    if (std::regex_match(name, m, rx("(hplate|rhplate)" + N + "x" + N + "x" + N + RA + RO))) {
        double w = num(m[2]), h = num(m[3]), c = num(m[4]);
        Ring p = m[1] == "hplate"
            ? Ring{{-w / 2, -h / 2}, {w / 2 - c, -h / 2}, {w / 2, 0}, {w / 2 - c, h / 2}, {-w / 2, h / 2}}
            : Ring{{-w / 2, -h / 2}, {w / 2, -h / 2}, {w / 2 - c, 0}, {w / 2, h / 2}, {-w / 2, h / 2}};
        p = dropRepeats(p);
        return one({fillet(p, cornerRadii(p, opt(m[5]), opt(m[6])))});
    }
    if (std::regex_match(name, m, rx("fhplate" + N + "x" + N + "x" + N + "x" + N + RA + RO))) {
        double w = num(m[1]), h = num(m[2]), vc = num(m[3]), hc = num(m[4]);
        Ring p = dropRepeats({{-w / 2, -h / 2}, {w / 2 - hc, -h / 2}, {w / 2, -h / 2 + vc}, {w / 2, h / 2 - vc}, {w / 2 - hc, h / 2}, {-w / 2, h / 2}});
        return one({fillet(p, cornerRadii(p, opt(m[5]), opt(m[6])))});
    }
    // radiused inverted home plate: a half-circle bite ms across in the middle of the right side;
    // ra rounds the four rectangle corners
    if (std::regex_match(name, m, rx("radhplate" + N + "x" + N + "x" + N + RA))) {
        double w = num(m[1]), h = num(m[2]), r = std::min(num(m[3]) / 2, h / 2), ra = opt(m[4]);
        Ring p{{-w / 2, -h / 2}, {w / 2, -h / 2}};
        int n = arcSteps(kPi);
        for (int k = 0; k <= n; k++) {  // clockwise about (w/2, 0): into the plate
            double t = -kPi / 2 - kPi * k / n;
            p.push_back({w / 2 + r * std::cos(t), r * std::sin(t)});
        }
        p.push_back({w / 2, h / 2});
        p.push_back({-w / 2, h / 2});
        std::vector<double> rad(p.size(), 0.0);
        rad[0] = rad[1] = rad[p.size() - 2] = rad[p.size() - 1] = ra;
        return one({fillet(p, rad)});
    }
    // radiused home plate (D shape): the right side an arc bulging r past the chord at w/2 - r
    if (std::regex_match(name, m, rx("dshape" + N + "x" + N + "x" + N + RA))) {
        double w = num(m[1]), h = num(m[2]), r = num(m[3]), ra = opt(m[4]);
        if (r <= 0) return one({fillet(rectRing(w, h), {ra, ra, ra, ra})});
        double R = (r * r + h * h / 4) / (2 * r), cx = w / 2 - R, phi = std::atan2(h / 2, R - r);
        Ring p{{-w / 2, -h / 2}};
        int n = arcSteps(2 * phi);
        for (int k = 0; k <= n; k++) p.push_back({cx + R * std::cos(-phi + 2 * phi * k / n), R * std::sin(-phi + 2 * phi * k / n)});
        p.push_back({-w / 2, h / 2});
        p = dropRepeats(p);
        std::vector<double> rad(p.size(), 0.0);
        rad.front() = rad.back() = ra;
        return one({fillet(p, rad)});
    }
    // cross: a horizontal line hs wide across w and a vertical one vs wide across h, crossing at
    // hc / vc percent of the width / height; round or square ends; ra rounds the inside corners
    if (std::regex_match(name, m, rx("cross" + N + "x" + N + "x" + N + "x" + N + "x" + N + "x" + N + R"(x([rs])(?:x?(?:ra)?(\d+(?:\.\d+)?))?)"))) {
        double w = num(m[1]), h = num(m[2]), hs = num(m[3]), vs = num(m[4]), ra = opt(m[8]);
        double xv = -w / 2 + std::stod(m[5].str()) / 100 * w, yh = -h / 2 + std::stod(m[6].str()) / 100 * h;
        bool round = m[7] == "r";
        Geom g;
        g.add.push_back({spanBar(-w / 2, yh, w / 2, yh, hs, round)});
        g.add.push_back({spanBar(xv, -h / 2, xv, h / 2, vs, round)});
        if (ra > 0)
            for (double sx : {-1.0, 1.0})
                for (double sy : {-1.0, 1.0}) {
                    double kx = xv + sx * vs / 2, ky = yh + sy * hs / 2;
                    double xEnd = w / 2 - (round ? hs / 2 : 0), yEnd = h / 2 - (round ? vs / 2 : 0);
                    if (std::abs(kx + sx * ra) <= xEnd && std::abs(ky + sy * ra) <= yEnd)
                        g.add.push_back({innerFillet(kx, ky, sx, sy, ra)});
                }
        return g;
    }
    // dogbone: lines hs wide along the top and bottom (w long) joined by a vertical one vs wide at
    // hc percent of the width; round or square ends; ra rounds the inside corners
    if (std::regex_match(name, m, rx("dogbone" + N + "x" + N + "x" + N + "x" + N + "x" + N + R"(x([rs])(?:x?(?:ra)?(\d+(?:\.\d+)?))?)"))) {
        double w = num(m[1]), h = num(m[2]), hs = num(m[3]), vs = num(m[4]), ra = opt(m[7]);
        double xv = -w / 2 + std::stod(m[5].str()) / 100 * w;
        bool round = m[6] == "r";
        Geom g;
        for (double sy : {-1.0, 1.0}) g.add.push_back({spanBar(-w / 2, sy * (h / 2 - hs / 2), w / 2, sy * (h / 2 - hs / 2), hs, round)});
        g.add.push_back({bar(xv, -h / 2 + hs / 2, 0, 1, 0, h - hs, vs / 2)});
        if (ra > 0)
            for (double sx : {-1.0, 1.0})
                for (double sy : {-1.0, 1.0}) {
                    double kx = xv + sx * vs / 2, ky = sy * (h / 2 - hs);
                    if (std::abs(kx + sx * ra) <= w / 2 - (round ? hs / 2 : 0))
                        g.add.push_back({innerFillet(kx, ky, sx, -sy, ra)});
                }
        return g;
    }
    // d-pack: hn x vn pads, (w - hg (hn - 1)) / hn by (h - vg (vn - 1)) / vn, corners rounded ra
    if (std::regex_match(name, m, rx("dpack" + N + "x" + N + "x" + N + "x" + N + "x" + N + "x" + N + R"((?:x(?:ra)?(\d+(?:\.\d+)?))?)"))) {
        double w = num(m[1]), h = num(m[2]), hg = num(m[3]), vg = num(m[4]), ra = opt(m[7]);
        int hn = std::max(1, spokes(m[5])), vn = std::max(1, spokes(m[6]));
        double pw = (w - hg * (hn - 1)) / hn, ph = (h - vg * (vn - 1)) / vn;
        Geom g;
        if (pw <= 0 || ph <= 0) return g;
        for (int i = 0; i < hn; i++)
            for (int j = 0; j < vn; j++) {
                double cx = -w / 2 + pw / 2 + i * (pw + hg), cy = -h / 2 + ph / 2 + j * (ph + vg);
                Ring r;
                for (auto [x, y] : cornerRect(pw, ph, ra, false, {1, 2, 3, 4})) r.push_back({cx + x, cy + y});
                g.add.push_back({r});
            }
        return g;
    }
    if (std::regex_match(name, m, rx("hole" + N + "(?:x.*)?")))
        return one({circleRing(num(m[1]) / 2)});
    return {};
}

OdbSymbol decodeSymbol(const std::string& name) {
    OdbSymbol sym;
    sym.name = name;
    // standard symbol names are lower case; some writers capitalise them (user symbols are looked
    // up by their lower-case name already)
    const std::string lname = toLower(name);

    // Rounded rect: rect<w>x<h>xr<r>
    std::regex rrectRe(R"(rect(\d+\.?\d*)x(\d+\.?\d*)xr(\d+\.?\d*))");
    std::smatch m;
    if (std::regex_match(lname, m, rrectRe)) {
        sym.shape = OdbSymbol::ROUNDRECT;
        sym.width = std::stod(m[1]) / 1000.0;
        sym.height = std::stod(m[2]) / 1000.0;
        sym.cornerRadius = std::stod(m[3]) / 1000.0;
        return sym;
    }

    // Plain rect: rect<w>x<h>
    std::regex rectRe(R"(rect(\d+\.?\d*)x(\d+\.?\d*))");
    if (std::regex_match(lname, m, rectRe)) {
        sym.shape = OdbSymbol::RECT;
        sym.width = std::stod(m[1]) / 1000.0;
        sym.height = std::stod(m[2]) / 1000.0;
        return sym;
    }

    // Oval: oval<w>x<h>
    std::regex ovalRe(R"(oval(\d+\.?\d*)x(\d+\.?\d*))");
    if (std::regex_match(lname, m, ovalRe)) {
        sym.shape = OdbSymbol::OVAL;
        sym.width = std::stod(m[1]) / 1000.0;
        sym.height = std::stod(m[2]) / 1000.0;
        return sym;
    }

    // Round: r<d>
    std::regex roundRe(R"(r(\d+\.?\d*))");
    if (std::regex_match(lname, m, roundRe)) {
        sym.shape = OdbSymbol::ROUND;
        sym.diameter = std::stod(m[1]) / 1000.0;
        sym.width = sym.diameter;
        sym.height = sym.diameter;
        return sym;
    }

    // Square: s<d>
    std::regex sqRe(R"(s(\d+\.?\d*))");
    if (std::regex_match(lname, m, sqRe)) {
        sym.shape = OdbSymbol::SQUARE;
        double side = std::stod(m[1]) / 1000.0;
        sym.width = side;
        sym.height = side;
        return sym;
    }

    // Donut round: donut_r<outer>x<inner>
    std::regex donutRoundRe(R"(donut_r(\d+\.?\d*)x(\d+\.?\d*))");
    if (std::regex_match(lname, m, donutRoundRe)) {
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
    if (std::regex_match(lname, m, donutRcRe)) {
        sym.shape = OdbSymbol::ROUNDRECT;
        sym.width = std::stod(m[1]) / 1000.0;
        sym.height = std::stod(m[2]) / 1000.0;
        sym.lineWidth = std::stod(m[3]) / 1000.0;
        sym.cornerRadius = std::stod(m[4]) / 1000.0;
        sym.donut = true;
        return sym;
    }

    Geom g = standardOutline(lname);
    sym.outline = std::move(g.fill);
    sym.outlineAdd = std::move(g.add);
    sym.outlineCut = std::move(g.cut);
    auto extent = [&](const std::vector<Ring>& rings) {  // its extent, so size comparisons and margins see it
        for (auto& ring : rings)
            for (auto& [x, y] : ring) {
                sym.width = std::max(sym.width, 2 * std::abs(x));
                sym.height = std::max(sym.height, 2 * std::abs(y));
            }
    };
    extent(sym.outline);
    for (auto& a : sym.outlineAdd) extent(a);
    return sym;
}

} // namespace odb2kicad
