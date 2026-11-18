#include "writer/panel.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>

namespace odb2kicad {

namespace {

const double kPi = 3.14159265358979323846;

struct Xform {
    double c = 1, s = 0, px = 0, py = 0, ax = 0, ay = 0, angle = 0;
    // y down: a clockwise turn on screen is the usual counter-clockwise matrix
    void apply(double& x, double& y) const {
        double dx = x - px, dy = y - py;
        x = ax + dx * c - dy * s;
        y = ay + dx * s + dy * c;
    }
    // KiCad angles turn counter-clockwise on screen
    double turn(double a) const {
        double r = std::fmod(a - angle, 360.0);
        if (r <= -180) r += 360;
        if (r > 180) r -= 360;
        if (std::abs(r) < 1e-9) r = 0;
        return r;
    }
};

std::string panelUuid(int instance, size_t index) {
    // the writer's uuids repeat their low 16 bits in groups 2-4; these never do
    char buf[64];
    unsigned v = (unsigned)index + 1;
    snprintf(buf, sizeof(buf), "%08x-%04x-7061-6e6c-%012x", v, (unsigned)instance & 0xffffu, v);
    return buf;
}

const KicadLayer* findLayer(const std::vector<KicadLayer>& layers, const std::string& name) {
    for (auto& l : layers)
        if (l.canonicalName == name) return &l;
    return nullptr;
}

} // namespace

bool hasBoardEdge(const KicadPcb& pcb) {
    auto onEdge = [](auto& items) {
        for (auto& i : items)
            if (i.layer == "Edge.Cuts") return true;
        return false;
    };
    return onEdge(pcb.grLines) || onEdge(pcb.grRects) || onEdge(pcb.grArcs) || onEdge(pcb.grPolys) || onEdge(pcb.grCircles);
}

void placeBoardInPanel(KicadPcb& panel, const KicadPcb& board, const PanelPlacement& at) {
    Xform t;
    double rad = at.angle * kPi / 180.0;
    t.c = std::cos(rad);
    t.s = std::sin(rad);
    if (std::abs(t.c) < 1e-12) t.c = 0;
    if (std::abs(t.s) < 1e-12) t.s = 0;
    t.px = at.pivotX; t.py = at.pivotY; t.ax = at.atX; t.ay = at.atY; t.angle = at.angle;
    auto pt = [&](double& x, double& y) { t.apply(x, y); };
    auto layerOf = [&](const std::string& l) { return l == "Edge.Cuts" ? at.edgeLayer : l; };

    // layers the copy uses that the panel has not got (the matrix is shared, but a document
    // layer may be drawn in one step and not another)
    bool added = false;
    auto need = [&](const std::string& name, const KicadLayer* from) {
        if (findLayer(panel.layers, name)) return;
        KicadLayer kl;
        if (from) kl = *from;
        else if (name == "Dwgs.User") kl = {40, "Dwgs.User", "user", "User.Drawings"};
        else return;
        for (auto& l : panel.layers)
            if (l.id == kl.id) return;
        panel.layers.push_back(kl);
        added = true;
    };
    for (auto& l : board.layers) need(l.canonicalName, &l);
    if (at.edgeLayer != "Edge.Cuts" && hasBoardEdge(board)) need(at.edgeLayer, findLayer(board.layers, at.edgeLayer));
    if (added)
        std::stable_sort(panel.layers.begin(), panel.layers.end(),
                         [](const KicadLayer& a, const KicadLayer& b) { return a.id < b.id; });

    // nets: the copy's own, after the panel's
    std::map<int, int> netId;
    std::map<int, std::string> netName;
    netId[0] = 0;
    netName[0] = "";
    int next = 0;
    for (auto& n : panel.nets) next = std::max(next, n.id);
    for (auto& n : board.nets) {
        if (n.id == 0) continue;
        KicadNet kn;
        kn.id = ++next;
        kn.name = at.netPrefix + n.name;
        netId[n.id] = kn.id;
        netName[n.id] = kn.name;
        panel.nets.push_back(kn);
    }
    auto net = [&](int id) { auto it = netId.find(id); return it == netId.end() ? 0 : it->second; };
    auto name = [&](int id, const std::string& was) {
        auto it = netName.find(id);
        return it == netName.end() ? (was.empty() ? was : at.netPrefix + was) : it->second;
    };

    size_t fpIndex = 0;
    for (auto fp : board.footprints) {
        pt(fp.x, fp.y);
        fp.angle = t.turn(fp.angle);
        fp.uuid = panelUuid(at.instance, fpIndex++);
        for (auto& pad : fp.pads) {
            pad.angle = t.turn(pad.angle);
            pad.netName = name(pad.netId, pad.netName);
            pad.netId = net(pad.netId);
            for (auto& ap : pad.apertures) {
                ap.angle = t.turn(ap.angle);
                ap.netName = name(ap.netId, ap.netName);
                ap.netId = net(ap.netId);
            }
        }
        panel.footprints.push_back(std::move(fp));
    }
    for (auto s : board.segments) {
        pt(s.x1, s.y1); pt(s.x2, s.y2);
        s.netId = net(s.netId);
        panel.segments.push_back(s);
    }
    for (auto a : board.arcs) {
        pt(a.xs, a.ys); pt(a.xm, a.ym); pt(a.xe, a.ye);
        a.netId = net(a.netId);
        panel.arcs.push_back(a);
    }
    for (auto v : board.vias) {
        pt(v.x, v.y);
        v.netId = net(v.netId);
        panel.vias.push_back(v);
    }
    for (auto l : board.grLines) {
        pt(l.x1, l.y1); pt(l.x2, l.y2);
        l.layer = layerOf(l.layer);
        panel.grLines.push_back(l);
    }
    const bool square = std::abs(t.s) < 1e-12 || std::abs(t.c) < 1e-12;  // a multiple of 90 degrees
    for (auto r : board.grRects) {
        if (square) {
            pt(r.x1, r.y1); pt(r.x2, r.y2);
            r.layer = layerOf(r.layer);
            panel.grRects.push_back(r);
            continue;
        }
        KicadGrPoly p;
        p.pts = {{r.x1, r.y1}, {r.x2, r.y1}, {r.x2, r.y2}, {r.x1, r.y2}};
        for (auto& [x, y] : p.pts) pt(x, y);
        p.width = r.width;
        p.filled = false;
        p.layer = layerOf(r.layer);
        panel.grPolys.push_back(p);
    }
    for (auto a : board.grArcs) {
        pt(a.xs, a.ys); pt(a.xm, a.ym); pt(a.xe, a.ye);
        a.layer = layerOf(a.layer);
        panel.grArcs.push_back(a);
    }
    for (auto p : board.grPolys) {
        for (auto& [x, y] : p.pts) pt(x, y);
        p.layer = layerOf(p.layer);
        panel.grPolys.push_back(p);
    }
    for (auto c : board.grCircles) {
        pt(c.cx, c.cy);
        c.layer = layerOf(c.layer);
        panel.grCircles.push_back(c);
    }
    // zone priorities only order zones that overlap: a copy's keep theirs
    for (auto z : board.zones) {
        for (auto& [x, y] : z.outlinePoints) pt(x, y);
        for (auto& poly : z.filledPolygons)
            for (auto& [x, y] : poly) pt(x, y);
        z.netName = name(z.netId, z.netName);
        z.netId = net(z.netId);
        panel.zones.push_back(std::move(z));
    }
}

} // namespace odb2kicad
