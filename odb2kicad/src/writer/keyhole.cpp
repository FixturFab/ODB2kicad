#include "writer/keyhole.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace odb2kicad {

static double signedArea(const KeyholeRing& pts) {
    double a = 0;
    for (size_t i = 0; i < pts.size(); i++) {
        auto& p = pts[i];
        auto& q = pts[(i + 1) % pts.size()];
        a += p.first * q.second - q.first * p.second;
    }
    return a / 2;
}

KeyholeRing keyholeReference(KeyholeRing outer, std::vector<KeyholeRing> holes) {
    for (auto& hole : holes) {
        if (hole.size() < 3 || outer.empty()) continue;
        if ((signedArea(hole) > 0) == (signedArea(outer) > 0)) std::reverse(hole.begin(), hole.end());
        size_t bi = 0, bj = 0;
        double best = 1e300;
        for (size_t i = 0; i < outer.size(); i++) {
            for (size_t j = 0; j < hole.size(); j++) {
                double dx = outer[i].first - hole[j].first, dy = outer[i].second - hole[j].second;
                double d = dx * dx + dy * dy;
                if (d < best) { best = d; bi = i; bj = j; }
            }
        }
        KeyholeRing merged(outer.begin(), outer.begin() + bi + 1);
        for (size_t k = 0; k <= hole.size(); k++) merged.push_back(hole[(bj + k) % hole.size()]);
        merged.insert(merged.end(), outer.begin() + bi, outer.end());
        outer.swap(merged);
    }
    return outer;
}

// The fast form keeps the outline as a linked list of points (a splice goes in after one point)
// with order labels (which point comes first, for ties), and every point in a uniform grid, so a
// hole's closest pair is found by searching the cells around each of its points instead of
// trying the whole outline. The winding test keeps a running area and only recomputes it in the
// reference's own order when the running value is too close to zero to trust its sign.
namespace {

struct Node {
    double x, y;
    int next;          // -1: the last point
    uint64_t label;    // increasing along the list
};

constexpr uint64_t kTopLabel = uint64_t(1) << 62;

struct Outline {
    std::vector<Node> nodes;
    // grid
    double minX = 0, minY = 0, cell = 1;
    int nx = 1, ny = 1;
    std::vector<std::vector<int>> cells;
    // twice the signed area, and a bound on its rounding (sum of the terms' magnitudes)
    double area2 = 0, areaMag = 0;

    int cellX(double x) const { return std::clamp((int)std::floor((x - minX) / cell), 0, nx - 1); }
    int cellY(double y) const { return std::clamp((int)std::floor((y - minY) / cell), 0, ny - 1); }
    void index(int k) { cells[(size_t)cellY(nodes[k].y) * nx + cellX(nodes[k].x)].push_back(k); }

    void edge(int a, int b, double sign) {
        const Node& p = nodes[a];
        const Node& q = nodes[b];
        double u = p.x * q.y, v = q.x * p.y;
        area2 += sign * (u - v);
        areaMag += std::abs(u) + std::abs(v);
    }

    KeyholeRing points() const {
        KeyholeRing out;
        out.reserve(nodes.size());
        for (int k = 0; k != -1; k = nodes[k].next) out.push_back({nodes[k].x, nodes[k].y});
        return out;
    }

    bool positive() const {
        if (areaMag > 0 && std::abs(area2) > 1e-6 * areaMag) return area2 > 0;
        return signedArea(points()) > 0;
    }

    void relabel(size_t extra) {
        uint64_t step = kTopLabel / (uint64_t)(nodes.size() + extra + 2);
        uint64_t l = 0;
        for (int k = 0; k != -1; k = nodes[k].next) nodes[k].label = (l += step);
    }
};

} // namespace

KeyholeRing keyhole(KeyholeRing outer, std::vector<KeyholeRing> holes) {
    if (outer.empty()) return outer;
    // small islands: trying every pair is quicker than building the grid
    double pairs = 0, size = (double)outer.size();
    size_t total = outer.size();
    for (auto& h : holes) {
        if (h.size() < 3) continue;
        pairs += size * (double)h.size();
        size += (double)h.size() + 2;
        total += h.size() + 2;
    }
    if (pairs < 250000) return keyholeReference(std::move(outer), std::move(holes));

    // the grid covers every point the outline will have; coordinates beyond what squares safely
    // (or not numbers) take the reference's path, which is exact for them by definition
    double minX = outer[0].first, maxX = minX, minY = outer[0].second, maxY = minY;
    auto grow = [&](const KeyholeRing& r) {
        for (auto& [x, y] : r) {
            minX = std::min(minX, x); maxX = std::max(maxX, x);
            minY = std::min(minY, y); maxY = std::max(maxY, y);
        }
    };
    grow(outer);
    for (auto& h : holes)
        if (h.size() >= 3) grow(h);
    auto sane = [](double v) { return std::isfinite(v) && std::abs(v) < 1e100; };
    if (!sane(minX) || !sane(maxX) || !sane(minY) || !sane(maxY))
        return keyholeReference(std::move(outer), std::move(holes));
    for (auto& [x, y] : outer)
        if (!sane(x) || !sane(y)) return keyholeReference(std::move(outer), std::move(holes));
    for (auto& h : holes)
        for (auto& [x, y] : h)
            if (!sane(x) || !sane(y)) return keyholeReference(std::move(outer), std::move(holes));

    Outline o;
    o.minX = minX;
    o.minY = minY;
    double w = maxX - minX, h = maxY - minY;
    double target = std::max(1.0, (double)total / 2);  // about two points a cell
    double cell = (w > 0 && h > 0) ? std::sqrt(w * h / target) : std::max(w, h) / target;
    // not finer than the coordinates' own rounding can tell apart (the search's distance bound
    // counts whole cells)
    double span = std::max({std::abs(minX), std::abs(maxX), std::abs(minY), std::abs(maxY), 1e-300});
    cell = std::max(cell, 1e-6 * span);
    if (!(cell > 0) || !std::isfinite(cell)) cell = 1;
    // a thin box: fewer, wider cells
    while ((std::floor(w / cell) + 1) * (std::floor(h / cell) + 1) > 4.0 * (double)total + 16) cell *= 2;
    o.cell = cell;
    o.nx = (int)std::floor(w / cell) + 1;  // cellX(maxX) is nx - 1: the grid covers every point
    o.ny = (int)std::floor(h / cell) + 1;
    o.cells.resize((size_t)o.nx * o.ny);
    o.nodes.reserve(total);
    for (size_t i = 0; i < outer.size(); i++) {
        o.nodes.push_back({outer[i].first, outer[i].second, i + 1 < outer.size() ? (int)i + 1 : -1, 0});
        o.index((int)i);
    }
    o.relabel(0);
    for (size_t i = 0; i < outer.size(); i++) o.edge((int)i, (int)((i + 1) % outer.size()), 1);

    std::vector<std::pair<int, size_t>> ties;  // (outline point, hole point) at the best distance
    for (auto& hole : holes) {
        if (hole.size() < 3) continue;
        if ((signedArea(hole) > 0) == o.positive()) std::reverse(hole.begin(), hole.end());

        double best = 1e300;
        ties.clear();
        const int rings = std::max(o.nx, o.ny);
        for (size_t j = 0; j < hole.size(); j++) {
            const double hx = hole[j].first, hy = hole[j].second;
            const int cx = o.cellX(hx), cy = o.cellY(hy);
            for (int r = 0; r <= rings; r++) {
                if (r >= 2) {
                    // every point of ring r is at least r - 1 cells away; the margin keeps ties
                    double lb = (r - 1) * o.cell;
                    if (lb * lb * (1 - 1e-6) > best) break;
                }
                const int x0 = cx - r, x1 = cx + r, y0 = cy - r, y1 = cy + r;
                // a ring wholly outside the grid: so is every later one
                if (r > 0 && x0 < 0 && y0 < 0 && x1 >= o.nx && y1 >= o.ny) break;
                auto scan = [&](int gx, int gy) {
                    if (gx < 0 || gy < 0 || gx >= o.nx || gy >= o.ny) return;
                    for (int k : o.cells[(size_t)gy * o.nx + gx]) {
                        const Node& n = o.nodes[k];
                        double dx = n.x - hx, dy = n.y - hy;
                        double d = dx * dx + dy * dy;
                        if (d < best) {
                            best = d;
                            ties.clear();
                            ties.push_back({k, j});
                        } else if (d == best) {
                            ties.push_back({k, j});
                        }
                    }
                };
                if (r == 0) { scan(cx, cy); continue; }
                for (int gx = std::max(x0, 0); gx <= std::min(x1, o.nx - 1); gx++) { scan(gx, y0); scan(gx, y1); }
                for (int gy = std::max(y0 + 1, 0); gy <= std::min(y1 - 1, o.ny - 1); gy++) { scan(x0, gy); scan(x1, gy); }
            }
        }
        // the reference's winner: the earliest outline point, then the earliest hole point
        int bi = 0;
        size_t bj = 0;
        if (!ties.empty()) {
            auto win = ties[0];
            for (auto& t : ties) {
                uint64_t lt = o.nodes[t.first].label, lw = o.nodes[win.first].label;
                if (lt < lw || (lt == lw && t.second < win.second)) win = t;
            }
            bi = win.first;
            bj = win.second;
        }

        // splice after bi: the hole from bj round to bj again, then bi once more
        const size_t m = hole.size() + 2;
        const int after = o.nodes[bi].next;
        const int wrapTo = after == -1 ? 0 : after;
        uint64_t lo = o.nodes[bi].label, hi = after == -1 ? kTopLabel : o.nodes[after].label;
        if ((hi - lo) / (m + 1) == 0) {
            o.relabel(m);
            lo = o.nodes[bi].label;
            hi = after == -1 ? kTopLabel : o.nodes[after].label;
        }
        const uint64_t step = (hi - lo) / (m + 1);
        o.edge(bi, wrapTo, -1);
        int prev = bi;
        for (size_t k = 0; k < m; k++) {
            double x, y;
            if (k < hole.size() + 1) {
                x = hole[(bj + k) % hole.size()].first;
                y = hole[(bj + k) % hole.size()].second;
            } else {
                x = o.nodes[bi].x;
                y = o.nodes[bi].y;
            }
            int id = (int)o.nodes.size();
            o.nodes.push_back({x, y, -1, lo + step * (k + 1)});
            o.nodes[prev].next = id;
            o.edge(prev, id, 1);
            o.index(id);
            prev = id;
        }
        o.nodes[prev].next = after;
        o.edge(prev, wrapTo, 1);
    }
    return o.points();
}

} // namespace odb2kicad
