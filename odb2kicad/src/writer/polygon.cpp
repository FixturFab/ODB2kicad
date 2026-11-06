#include "writer/polygon.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <set>
#include <unordered_map>

namespace odb2kicad {
namespace poly {

namespace {

const double kGrid = 1e-6;     // mm per grid unit (1 nm)
const double kProbe = 2.0;     // grid units: how far off an edge its sides are sampled
const double kPi = 3.14159265358979323846;

struct IP {
    int64_t x, y;
    bool operator==(const IP& o) const { return x == o.x && y == o.y; }
    bool operator!=(const IP& o) const { return !(*this == o); }
    bool operator<(const IP& o) const { return x < o.x || (x == o.x && y < o.y); }
};
struct IPHash {
    size_t operator()(const IP& p) const { return std::hash<int64_t>()(p.x * 1000003LL ^ p.y); }
};

struct Edge {
    IP a, b;
    std::vector<IP> splits;
};

// A ring in grid units with a y-bucket index of its edges, for point-in-polygon tests
struct IndexedRing {
    std::vector<std::pair<double, double>> pts;
    double minX = 0, minY = 0, maxX = 0, maxY = 0;
    double bucketH = 1;
    std::vector<std::vector<int>> buckets;  // edges (by start index) spanning each y slab

    void build() {
        minX = minY = 1e300;
        maxX = maxY = -1e300;
        for (auto& [x, y] : pts) {
            minX = std::min(minX, x); maxX = std::max(maxX, x);
            minY = std::min(minY, y); maxY = std::max(maxY, y);
        }
        int nb = (int)std::clamp<size_t>(pts.size() / 4, 1, 512);
        bucketH = std::max((maxY - minY) / nb, 1e-9);
        buckets.assign(nb, {});
        size_t n = pts.size();
        for (size_t i = 0; i < n; i++) {
            double y0 = pts[i].second, y1 = pts[(i + 1) % n].second;
            int b0 = bucket(std::min(y0, y1)), b1 = bucket(std::max(y0, y1));
            for (int b = b0; b <= b1; b++) buckets[b].push_back((int)i);
        }
    }
    int bucket(double y) const {
        int b = (int)std::floor((y - minY) / bucketH);
        return std::clamp(b, 0, (int)buckets.size() - 1);
    }
    // even-odd crossing test
    bool contains(double x, double y) const {
        if (x < minX || x > maxX || y < minY || y > maxY) return false;
        bool in = false;
        size_t n = pts.size();
        for (int i : buckets[bucket(y)]) {
            auto& a = pts[i];
            auto& b = pts[(i + 1) % n];
            if ((a.second > y) != (b.second > y) &&
                x < (b.first - a.first) * (y - a.second) / (b.second - a.second) + a.first)
                in = !in;
        }
        return in;
    }
};

struct IndexedShape {
    std::vector<IndexedRing> rings;
    double minX = 1e300, minY = 1e300, maxX = -1e300, maxY = -1e300;
    bool contains(double x, double y) const {
        if (x < minX || x > maxX || y < minY || y > maxY) return false;
        bool in = false;
        for (auto& r : rings)
            if (r.contains(x, y)) in = !in;
        return in;
    }
};

// Shapes bucketed on a coarse grid, so a point only tests the shapes near it
struct ShapeSet {
    std::vector<IndexedShape> shapes;
    double ox = 0, oy = 0, cell = 1;
    int nx = 1, ny = 1;
    std::vector<std::vector<int>> cells;

    void build() {
        double minX = 1e300, minY = 1e300, maxX = -1e300, maxY = -1e300;
        for (auto& s : shapes) {
            minX = std::min(minX, s.minX); minY = std::min(minY, s.minY);
            maxX = std::max(maxX, s.maxX); maxY = std::max(maxY, s.maxY);
        }
        if (shapes.empty()) return;
        ox = minX; oy = minY;
        int side = (int)std::clamp(std::sqrt((double)shapes.size()) * 2, 1.0, 256.0);
        cell = std::max(std::max(maxX - minX, maxY - minY) / side, 1.0);
        nx = (int)((maxX - minX) / cell) + 1;
        ny = (int)((maxY - minY) / cell) + 1;
        cells.assign((size_t)nx * ny, {});
        for (int i = 0; i < (int)shapes.size(); i++) {
            auto& s = shapes[i];
            int x0 = cx(s.minX), x1 = cx(s.maxX), y0 = cy(s.minY), y1 = cy(s.maxY);
            for (int yy = y0; yy <= y1; yy++)
                for (int xx = x0; xx <= x1; xx++) cells[(size_t)yy * nx + xx].push_back(i);
        }
    }
    int cx(double x) const { return std::clamp((int)std::floor((x - ox) / cell), 0, nx - 1); }
    int cy(double y) const { return std::clamp((int)std::floor((y - oy) / cell), 0, ny - 1); }
    bool contains(double x, double y) const {
        if (shapes.empty()) return false;
        if (x < ox - 1 || y < oy - 1 || x > ox + nx * cell + 1 || y > oy + ny * cell + 1) return false;
        for (int i : cells[(size_t)cy(y) * nx + cx(x)])
            if (shapes[i].contains(x, y)) return true;
        return false;
    }
};

double cross(double ax, double ay, double bx, double by) { return ax * by - ay * bx; }

}  // namespace

double signedArea(const Ring& r) {
    double a = 0;
    for (size_t i = 0; i < r.size(); i++) {
        auto& p = r[i];
        auto& q = r[(i + 1) % r.size()];
        a += p.first * q.second - q.first * p.second;
    }
    return a / 2;
}

static void bbox(const Shape& s, double& x0, double& y0, double& x1, double& y1) {
    x0 = y0 = 1e300;
    x1 = y1 = -1e300;
    for (auto& r : s)
        for (auto& [x, y] : r) {
            x0 = std::min(x0, x); x1 = std::max(x1, x);
            y0 = std::min(y0, y); y1 = std::max(y1, y);
        }
}

bool boxesOverlap(const Shape& a, const Shape& b) {
    double ax0, ay0, ax1, ay1, bx0, by0, bx1, by1;
    bbox(a, ax0, ay0, ax1, ay1);
    bbox(b, bx0, by0, bx1, by1);
    return ax0 <= bx1 && bx0 <= ax1 && ay0 <= by1 && by0 <= ay1;
}

std::vector<Island> difference(const std::vector<Shape>& a, const std::vector<Shape>& b) {
    // local origin keeps the grid numbers small
    double ox = 1e300, oy = 1e300;
    for (auto* set : {&a, &b})
        for (auto& s : *set)
            for (auto& r : s)
                for (auto& [x, y] : r) { ox = std::min(ox, x); oy = std::min(oy, y); }
    if (ox > 1e299) return {};
    auto snap = [&](const Pt& p) {
        return IP{(int64_t)std::llround((p.first - ox) / kGrid), (int64_t)std::llround((p.second - oy) / kGrid)};
    };

    std::vector<Edge> edges;
    ShapeSet sa, sb;
    for (int side = 0; side < 2; side++) {
        auto& in = side ? b : a;
        auto& out = side ? sb : sa;
        for (auto& s : in) {
            IndexedShape is;
            for (auto& r : s) {
                std::vector<IP> ip;
                for (auto& p : r) {
                    IP q = snap(p);
                    if (ip.empty() || ip.back() != q) ip.push_back(q);
                }
                while (ip.size() > 1 && ip.back() == ip.front()) ip.pop_back();
                if (ip.size() < 3) continue;
                IndexedRing ir;
                for (size_t i = 0; i < ip.size(); i++) {
                    ir.pts.push_back({(double)ip[i].x, (double)ip[i].y});
                    edges.push_back({ip[i], ip[(i + 1) % ip.size()], {}});
                }
                ir.build();
                is.minX = std::min(is.minX, ir.minX); is.maxX = std::max(is.maxX, ir.maxX);
                is.minY = std::min(is.minY, ir.minY); is.maxY = std::max(is.maxY, ir.maxY);
                is.rings.push_back(std::move(ir));
            }
            if (!is.rings.empty()) out.shapes.push_back(std::move(is));
        }
        out.build();
    }

    // split every edge where another one crosses or touches it (sweep on x)
    std::vector<int> order(edges.size());
    for (size_t i = 0; i < order.size(); i++) order[i] = (int)i;
    auto minx = [&](int i) { return std::min(edges[i].a.x, edges[i].b.x); };
    auto maxx = [&](int i) { return std::max(edges[i].a.x, edges[i].b.x); };
    std::sort(order.begin(), order.end(), [&](int i, int j) { return minx(i) < minx(j); });
    auto onSeg = [](const Edge& e, const IP& p) {  // p within half a grid unit of e, strictly inside
        double dx = (double)(e.b.x - e.a.x), dy = (double)(e.b.y - e.a.y);
        double l2 = dx * dx + dy * dy;
        if (l2 == 0 || p == e.a || p == e.b) return false;
        double px = (double)(p.x - e.a.x), py = (double)(p.y - e.a.y);
        double t = (px * dx + py * dy) / l2;
        if (t <= 0 || t >= 1) return false;
        return std::abs(cross(dx, dy, px, py)) / std::sqrt(l2) <= 0.5;
    };
    for (size_t oi = 0; oi < order.size(); oi++) {
        int i = order[oi];
        Edge& e = edges[i];
        int64_t ex1 = maxx(i);
        int64_t ey0 = std::min(e.a.y, e.b.y), ey1 = std::max(e.a.y, e.b.y);
        for (size_t oj = oi + 1; oj < order.size(); oj++) {
            int j = order[oj];
            if (minx(j) > ex1) break;
            Edge& f = edges[j];
            if (std::max(f.a.y, f.b.y) < ey0 || std::min(f.a.y, f.b.y) > ey1) continue;
            // endpoints of one lying on the other (T junctions, collinear overlaps)
            bool touched = false;
            for (const IP& p : {f.a, f.b})
                if (onSeg(e, p)) { e.splits.push_back(p); touched = true; }
            for (const IP& p : {e.a, e.b})
                if (onSeg(f, p)) { f.splits.push_back(p); touched = true; }
            if (touched) continue;
            double rx = (double)(e.b.x - e.a.x), ry = (double)(e.b.y - e.a.y);
            double sx = (double)(f.b.x - f.a.x), sy = (double)(f.b.y - f.a.y);
            double d = cross(rx, ry, sx, sy);
            if (d == 0) continue;
            double qx = (double)(f.a.x - e.a.x), qy = (double)(f.a.y - e.a.y);
            double t = cross(qx, qy, sx, sy) / d, u = cross(qx, qy, rx, ry) / d;
            if (t <= 0 || t >= 1 || u <= 0 || u >= 1) continue;
            IP p{(int64_t)std::llround(e.a.x + t * rx), (int64_t)std::llround(e.a.y + t * ry)};
            if (p != e.a && p != e.b) e.splits.push_back(p);
            if (p != f.a && p != f.b) f.splits.push_back(p);
        }
    }

    // sub-edges, each once (undirected)
    std::set<std::pair<IP, IP>> pieces;
    for (auto& e : edges) {
        std::vector<IP> pts = e.splits;
        pts.push_back(e.a);
        pts.push_back(e.b);
        double dx = (double)(e.b.x - e.a.x), dy = (double)(e.b.y - e.a.y);
        std::sort(pts.begin(), pts.end(), [&](const IP& p, const IP& q) {
            return (p.x - e.a.x) * dx + (p.y - e.a.y) * dy < (q.x - e.a.x) * dx + (q.y - e.a.y) * dy;
        });
        pts.erase(std::unique(pts.begin(), pts.end()), pts.end());
        for (size_t k = 0; k + 1 < pts.size(); k++) {
            IP p = pts[k], q = pts[k + 1];
            if (q < p) std::swap(p, q);
            pieces.insert({p, q});
        }
    }

    // keep a piece when the result (in a, not in b) differs on its two sides; result on its left
    std::vector<std::pair<IP, IP>> kept;
    for (auto& [p, q] : pieces) {
        double dx = (double)(q.x - p.x), dy = (double)(q.y - p.y), l = std::hypot(dx, dy);
        if (l == 0) continue;
        double mx = (p.x + q.x) / 2.0, my = (p.y + q.y) / 2.0;
        double nx = -dy / l * kProbe, ny = dx / l * kProbe;
        bool left = sa.contains(mx + nx, my + ny) && !sb.contains(mx + nx, my + ny);
        bool right = sa.contains(mx - nx, my - ny) && !sb.contains(mx - nx, my - ny);
        if (left == right) continue;
        kept.push_back(left ? std::make_pair(p, q) : std::make_pair(q, p));
    }

    // chain into rings, taking the sharpest left turn where several pieces leave a point
    std::unordered_map<IP, std::vector<int>, IPHash> from;
    for (int i = 0; i < (int)kept.size(); i++) from[kept[i].first].push_back(i);
    std::vector<char> used(kept.size(), 0);
    std::vector<Ring> rings;
    for (int s = 0; s < (int)kept.size(); s++) {
        if (used[s]) continue;
        std::vector<IP> ring;
        int cur = s;
        while (cur >= 0 && !used[cur]) {
            used[cur] = 1;
            ring.push_back(kept[cur].first);
            const IP& at = kept[cur].second;
            if (at == kept[s].first) break;
            double ix = (double)(at.x - kept[cur].first.x), iy = (double)(at.y - kept[cur].first.y);
            int next = -1;
            double best = -1e9;
            auto it = from.find(at);
            if (it == from.end()) break;
            for (int c : it->second) {
                if (used[c]) continue;
                double ox2 = (double)(kept[c].second.x - at.x), oy2 = (double)(kept[c].second.y - at.y);
                double turn = std::atan2(cross(ix, iy, ox2, oy2), ix * ox2 + iy * oy2);
                if (turn > best) { best = turn; next = c; }
            }
            cur = next;
        }
        // drop collinear vertices left by the splits
        std::vector<IP> clean;
        size_t n = ring.size();
        for (size_t k = 0; k < n; k++) {
            const IP& p0 = ring[(k + n - 1) % n];
            const IP& p1 = ring[k];
            const IP& p2 = ring[(k + 1) % n];
            double c = cross((double)(p1.x - p0.x), (double)(p1.y - p0.y), (double)(p2.x - p1.x), (double)(p2.y - p1.y));
            double l = std::hypot((double)(p2.x - p0.x), (double)(p2.y - p0.y));
            if (l > 0 && std::abs(c) / l < 0.25 &&
                (p1.x - p0.x) * (p2.x - p1.x) + (p1.y - p0.y) * (p2.y - p1.y) > 0) continue;
            clean.push_back(p1);
        }
        if (clean.size() < 3) continue;
        Ring r;
        for (auto& p : clean) r.push_back({p.x * kGrid + ox, p.y * kGrid + oy});
        if (std::abs(signedArea(r)) < 1e-12) continue;
        rings.push_back(std::move(r));
    }

    // nest: every hole (negative area) goes to the smallest island holding a point just outside it
    std::vector<Island> islands;
    std::vector<std::pair<double, int>> outerByArea;
    for (auto& r : rings)
        if (signedArea(r) > 0) {
            outerByArea.push_back({signedArea(r), (int)islands.size()});
            islands.push_back({r, {}});
        }
    std::sort(outerByArea.begin(), outerByArea.end());
    std::vector<IndexedRing> outerIdx(islands.size());
    for (size_t i = 0; i < islands.size(); i++) {
        for (auto& [x, y] : islands[i].outer) outerIdx[i].pts.push_back({x, y});
        outerIdx[i].build();
    }
    for (auto& r : rings) {
        if (signedArea(r) > 0) continue;
        double dx = r[1].first - r[0].first, dy = r[1].second - r[0].second, l = std::hypot(dx, dy);
        if (l == 0) continue;
        double px = (r[0].first + r[1].first) / 2 - dy / l * kProbe * kGrid;
        double py = (r[0].second + r[1].second) / 2 + dx / l * kProbe * kGrid;
        for (auto& [area, idx] : outerByArea) {
            if (outerIdx[idx].contains(px, py)) {
                islands[idx].holes.push_back(r);
                break;
            }
        }
    }
    return islands;
}

int circleSegments(double r) {
    const double err = 0.005;
    if (r <= err) return 16;
    int n = (int)std::ceil(kPi / std::acos(1 - err / r));
    return std::clamp(n, 16, 360);
}

Ring circle(double cx, double cy, double r) {
    Ring out;
    int n = circleSegments(r);
    for (int i = 0; i < n; i++) {
        double a = 2 * kPi * i / n;
        out.push_back({cx + r * std::cos(a), cy + r * std::sin(a)});
    }
    return out;
}

// Half turn of radius h about e, from e + h*u to e - h*u through e + h*t (u, t unit, at right
// angles); square ends turn through the two corners instead.
static void semi(Ring& out, double ex, double ey, double ux, double uy, double tx, double ty, double h,
                 bool square) {
    if (square) {
        out.push_back({ex + h * (ux + tx), ey + h * (uy + ty)});
        out.push_back({ex + h * (-ux + tx), ey + h * (-uy + ty)});
        return;
    }
    int n = std::max(8, circleSegments(h) / 2);
    for (int i = 0; i <= n; i++) {
        double f = kPi * i / n, c = std::cos(f), s = std::sin(f);
        out.push_back({ex + h * (c * ux + s * tx), ey + h * (c * uy + s * ty)});
    }
}

Ring stroke(double x1, double y1, double x2, double y2, double w, bool squareEnds) {
    double h = w / 2, dx = x2 - x1, dy = y2 - y1, l = std::hypot(dx, dy);
    if (l < 1e-9) {
        if (!squareEnds) return circle(x1, y1, h);
        return {{x1 - h, y1 - h}, {x1 + h, y1 - h}, {x1 + h, y1 + h}, {x1 - h, y1 + h}};
    }
    dx /= l; dy /= l;
    Ring out;
    semi(out, x2, y2, dy, -dx, dx, dy, h, squareEnds);    // right side -> left side round the end
    semi(out, x1, y1, -dy, dx, -dx, -dy, h, squareEnds);  // left side -> right side round the start
    return out;
}

Shape arcStroke(double xs, double ys, double xe, double ye, double xc, double yc, bool ccw, double w,
                bool squareEnds) {
    double r = std::hypot(xs - xc, ys - yc), h = w / 2;
    double a0 = std::atan2(ys - yc, xs - xc), a1 = std::atan2(ye - yc, xe - xc);
    if (std::abs(xs - xe) < 1e-9 && std::abs(ys - ye) < 1e-9) {  // full circle: a ring
        Shape s{circle(xc, yc, r + h)};
        if (r - h > 1e-9) s.push_back(circle(xc, yc, r - h));
        return s;
    }
    double sweep = a1 - a0;
    if (ccw) { while (sweep <= 0) sweep += 2 * kPi; }
    else { while (sweep >= 0) sweep -= 2 * kPi; }
    int n = std::max(2, (int)std::ceil(std::abs(sweep) / (2 * kPi) * circleSegments(r + h)));
    double ri = std::max(r - h, 0.0), sg = ccw ? 1 : -1;
    Ring out;
    for (int i = 0; i <= n; i++) {  // outer edge, start -> end
        double a = a0 + sweep * i / n;
        out.push_back({xc + (r + h) * std::cos(a), yc + (r + h) * std::sin(a)});
    }
    // round the end, outward along the direction of travel, to the inner edge
    semi(out, xe, ye, std::cos(a1), std::sin(a1), -std::sin(a1) * sg, std::cos(a1) * sg, h, squareEnds);
    for (int i = n; i >= 0; i--) {  // inner edge, end -> start
        double a = a0 + sweep * i / n;
        out.push_back({xc + ri * std::cos(a), yc + ri * std::sin(a)});
    }
    semi(out, xs, ys, -std::cos(a0), -std::sin(a0), std::sin(a0) * sg, -std::cos(a0) * sg, h, squareEnds);
    return Shape{out};
}

}  // namespace poly
}  // namespace odb2kicad
