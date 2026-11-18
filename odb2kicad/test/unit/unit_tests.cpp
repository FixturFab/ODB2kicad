// odb2kicad unit tests: one or more cases per defect found by round-tripping KiCad boards
// through kicad-cli's ODB++ export.
// Designs are built in memory (writer cases) or written as tiny ODB++ files (parser cases).
//
//   ./test/run_unit_tests.sh            (g++ build + run, no cmake needed)
//   cmake --build build && ctest --test-dir build
#include "model/odb_design.h"
#include "model/kicad_pcb.h"
#include "odb_parser/components_parser.h"
#include "odb_parser/eda_parser.h"
#include "odb_parser/features_parser.h"
#include "odb_parser/font_parser.h"
#include "odb_parser/profile_parser.h"
#include "odb_parser/info_parser.h"
#include "odb_parser/matrix_parser.h"
#include "odb_parser/netlist_parser.h"
#include "odb_parser/odb_parser.h"
#include "odb_parser/attrlist_parser.h"
#include "odb_parser/stephdr_parser.h"
#include "odb_parser/symbols.h"
#include "odb_parser/tools_parser.h"
#include "odb2kicad_lib.h"
#include "util/odb_file.h"
#include "writer/kicad_writer.h"
#include "writer/keyhole.h"
#include "writer/panel.h"
#include "writer/polygon.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <vector>

using namespace odb2kicad;

// ---------------------------------------------------------------- tiny test runner

static int gFailures = 0;
static std::vector<std::pair<std::string, std::function<void()>>>& registry() {
    static std::vector<std::pair<std::string, std::function<void()>>> r;
    return r;
}
struct Register {
    Register(const char* name, std::function<void()> fn) { registry().push_back({name, fn}); }
};
#define TEST(name) \
    static void name(); \
    static Register reg_##name(#name, name); \
    static void name()
#define CHECK(cond) do { if (!(cond)) { \
    std::cerr << "    " << __FILE__ << ":" << __LINE__ << ": CHECK(" #cond ") failed\n"; gFailures++; } } while (0)
#define CHECK_EQ(a, b) do { auto _a = (a); auto _b = (b); if (!(_a == _b)) { \
    std::cerr << "    " << __FILE__ << ":" << __LINE__ << ": " #a " == " #b " failed: [" << _a << "] vs [" << _b << "]\n"; \
    gFailures++; } } while (0)
#define CHECK_NEAR(a, b, tol) do { double _a = (a), _b = (b); if (std::abs(_a - _b) > (tol)) { \
    std::cerr << "    " << __FILE__ << ":" << __LINE__ << ": " #a " ~= " #b " failed: " << _a << " vs " << _b << "\n"; \
    gFailures++; } } while (0)

// ---------------------------------------------------------------- design builders

static OdbLayer layer(int row, const std::string& type, const std::string& name) {
    OdbLayer l;
    l.row = row;
    l.context = "BOARD";
    l.type = type;
    l.name = name;
    l.polarity = "POSITIVE";
    return l;
}

// A KiCad-style 2-layer stack: F.CU / B.CU copper, masks, pastes, plated + non-plated drill layers
static OdbDesign kicadDesign() {
    OdbDesign d;
    d.layers = {
        layer(1, "COMPONENT", "COMP_+_TOP"), layer(2, "SOLDER_PASTE", "F.PASTE"),
        layer(3, "SOLDER_MASK", "F.MASK"), layer(4, "SIGNAL", "F.CU"),
        layer(5, "DIELECTRIC", "DIELECTRIC_1"), layer(6, "SIGNAL", "B.CU"),
        layer(7, "SOLDER_MASK", "B.MASK"), layer(8, "SOLDER_PASTE", "B.PASTE"),
        layer(9, "DOCUMENT", "EDGE.CUTS"), layer(10, "DRILL", "DRILL_PLATED_F.CU-B.CU"),
        layer(11, "DRILL", "DRILL_NON-PLATED_F.CU-B.CU"), layer(12, "COMPONENT", "COMP_+_BOT"),
    };
    d.topComponents.units = "MM";
    d.edaData.nets = {{"$NONE$"}, {"GND"}, {"3V3"}};
    return d;
}

static int symbol(OdbFeatures& f, const std::string& name, OdbSymbol::Shape shape, double w, double h = -1) {
    OdbSymbol s;
    s.name = name;
    s.shape = shape;
    s.width = w;
    s.height = h < 0 ? w : h;
    if (shape == OdbSymbol::ROUND) s.diameter = w;
    f.symbols.push_back(s);
    return (int)f.symbols.size() - 1;
}

static int attrDef(OdbFeatures& f, const std::string& name) {
    for (auto& [i, n] : f.attrDefs) if (n == name) return i;
    int idx = (int)f.attrDefs.size();
    f.attrDefs[idx] = name;
    return idx;
}

// Flash a pad; extraAttrs are (attribute name, value) pairs
static void flash(OdbDesign& d, const std::string& layerName, double x, double y, const std::string& sym,
                  OdbSymbol::Shape shape, double w, double h = -1,
                  std::vector<std::pair<std::string, std::string>> extraAttrs = {}, double angle = 0, int mirror = 8) {
    auto& f = d.layerFeatures[layerName];
    f.units = "MM";
    int si = -1;
    for (int i = 0; i < (int)f.symbols.size(); i++) if (f.symbols[i].name == sym) si = i;
    if (si < 0) si = symbol(f, sym, shape, w, h);
    OdbPad p;
    p.x = x;
    p.y = y;
    p.symIdx = si;
    p.polarity = "P";
    p.mirror = mirror;
    p.angle = angle;
    for (auto& [n, v] : extraAttrs) p.attrs[attrDef(f, n)] = v;
    f.pads.push_back(p);
}

static OdbComponent& component(OdbDesign& d, const std::string& ref, double x, double y, int mountType,
                               bool bottom = false, double rotation = 0) {
    auto& comps = bottom ? d.botComponents : d.topComponents;
    comps.units = "MM";
    OdbComponent c;
    c.refdes = ref;
    c.footprint = "FP_" + ref;
    c.x = x;
    c.y = y;
    c.rotation = rotation;
    c.mirror = bottom ? "M" : "N";
    c.mountType = mountType;
    comps.components.push_back(c);
    return comps.components.back();
}

static void terminal(OdbComponent& c, const std::string& pin, double x, double y, int netIdx, double rotation = 0) {
    OdbTerminal t;
    t.pinIdx = (int)c.terminals.size();
    t.pinNum = pin;
    t.x = x;
    t.y = y;
    t.rotation = rotation;
    t.mirror = c.mirror;
    t.netIdx = netIdx;
    c.terminals.push_back(t);
}

// A round-pen line (a track) on a layer, width w (mm); netIdx >= 0 also gives it an FID record
static void stroke(OdbDesign& d, const std::string& layerName, double x1, double y1, double x2, double y2,
                   double w, int netIdx = -1) {
    auto& f = d.layerFeatures[layerName];
    f.units = "MM";
    OdbLine l;
    l.featIdx = (int)(f.pads.size() + f.lines.size() + f.arcs.size() + f.surfaces.size());
    l.x1 = x1; l.y1 = y1; l.x2 = x2; l.y2 = y2;
    l.symIdx = symbol(f, "r" + std::to_string((int)std::lround(w * 1000)), OdbSymbol::ROUND, w);
    l.polarity = "P";
    f.lines.push_back(l);
    if (netIdx >= 0) {
        int li = -1;
        for (int i = 0; i < (int)d.edaData.layers.size(); i++) if (d.edaData.layers[i] == layerName) li = i;
        if (li < 0) { d.edaData.layers.push_back(layerName); li = (int)d.edaData.layers.size() - 1; }
        OdbEdaFid fid;
        fid.layerIdx = li;
        fid.featIdx = l.featIdx;
        fid.netIdx = netIdx;
        d.edaData.fids.push_back(fid);
    }
}

static const KicadPad* findPad(const KicadPcb& pcb, const std::string& ref, const std::string& pin) {
    for (auto& fp : pcb.footprints)
        if (fp.refdes == ref)
            for (auto& p : fp.pads)
                if (p.pinNum == pin) return &p;
    return nullptr;
}

static bool hasLayer(const KicadPad& p, const std::string& l) {
    for (auto& x : p.layerNames) if (x == l) return true;
    return false;
}

// Square contour (OB + 4 OS back to the start), island or hole
static OdbContour square(double x0, double y0, double x1, double y1, bool hole) {
    OdbContour c;
    c.isHole = hole;
    c.points = {{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}, {x0, y0}};
    return c;
}

static std::string writeString(const KicadPcb& pcb) {
    std::ostringstream oss;
    writeKicadPcb(oss, pcb);
    return oss.str();
}

static void writeFile(const std::string& path, const std::string& text) {
    std::string dir = path.substr(0, path.rfind('/'));
    std::string cmd = "mkdir -p '" + dir + "'";
    if (std::system(cmd.c_str()) != 0) std::cerr << "mkdir failed: " << dir << "\n";
    std::ofstream(path) << text;
}

static std::string tempDir(const std::string& name) {
    const char* base = std::getenv("TMPDIR");
    std::string dir = std::string(base ? base : "/tmp") + "/odb2kicad-unit-" + name;
    std::string cmd = "rm -rf '" + dir + "'";
    if (std::system(cmd.c_str()) != 0) std::cerr << "rm failed: " << dir << "\n";
    return dir;
}

// ---------------------------------------------------------------- TH pads from drill records

// A through-hole connector whose component is tagged "other" (.comp_mount_type 0): the plated
// drill hit makes each pad thru_hole with its drill, on all copper layers, mask from the mask layers,
// no paste. The SMD pad of the same part (no drill hit) stays smd with its paste opening.
TEST(th_pad_type_comes_from_plated_drill_hit) {
    OdbDesign d = kicadDesign();
    auto& j1 = component(d, "J1", 10, -10, /*mountType=*/0);
    terminal(j1, "1", 10, -10, 1);
    terminal(j1, "2", 12.54, -10, 2);
    terminal(j1, "3", 15, -10, 2);  // SMD tab
    for (const char* cu : {"f.cu", "b.cu"}) {
        flash(d, cu, 10, -10, "rect1700.0x1700.0", OdbSymbol::RECT, 1.7);
        flash(d, cu, 12.54, -10, "r1700.0", OdbSymbol::ROUND, 1.7);
    }
    flash(d, "f.cu", 15, -10, "rect2000.0x1000.0", OdbSymbol::RECT, 2.0, 1.0);
    for (const char* m : {"f.mask", "b.mask"}) {
        flash(d, m, 10, -10, "rect1800.0x1800.0", OdbSymbol::RECT, 1.8);
        flash(d, m, 12.54, -10, "r1800.0", OdbSymbol::ROUND, 1.8);
    }
    flash(d, "f.mask", 15, -10, "rect2100.0x1100.0", OdbSymbol::RECT, 2.1, 1.1);
    flash(d, "f.paste", 15, -10, "rect2000.0x1000.0", OdbSymbol::RECT, 2.0, 1.0);
    flash(d, "drill_plated_f.cu-b.cu", 10, -10, "r1000.0", OdbSymbol::ROUND, 1.0, -1, {{".drill", "0"}});
    flash(d, "drill_plated_f.cu-b.cu", 12.54, -10, "r1000.0", OdbSymbol::ROUND, 1.0, -1, {{".drill", "0"}});

    KicadPcb pcb = transformToKicad(d);
    const KicadPad* p1 = findPad(pcb, "J1", "1");
    const KicadPad* p2 = findPad(pcb, "J1", "2");
    const KicadPad* p3 = findPad(pcb, "J1", "3");
    CHECK(p1 && p2 && p3);
    if (!p1 || !p2 || !p3) return;
    CHECK_EQ(p1->type, std::string("thru_hole"));
    CHECK_EQ(p1->shape, std::string("rect"));
    CHECK_NEAR(p1->drill, 1.0, 1e-9);
    CHECK(hasLayer(*p1, "*.Cu") && hasLayer(*p1, "F.Mask") && hasLayer(*p1, "B.Mask"));
    CHECK(!hasLayer(*p1, "F.Paste") && !hasLayer(*p1, "B.Paste"));
    CHECK_EQ(p2->type, std::string("thru_hole"));
    CHECK_NEAR(p2->drill, 1.0, 1e-9);
    CHECK_EQ(p3->type, std::string("smd"));
    CHECK(hasLayer(*p3, "F.Cu") && hasLayer(*p3, "F.Paste") && hasLayer(*p3, "F.Mask"));
    CHECK(!hasLayer(*p3, "B.Cu"));
    CHECK_NEAR(p3->drill, 0.0, 1e-9);
    // the written pad carries its drill
    std::string out = writeString(pcb);
    CHECK(out.find("(pad \"1\" thru_hole rect") != std::string::npos);
    CHECK(out.find("(drill 1)") != std::string::npos);
}

// A component tagged through-hole (.comp_mount_type 2) whose pad has no drill hit is SMD:
// the tag no longer forces a plated hole with no drill.
TEST(th_tagged_component_without_drill_hit_is_smd) {
    OdbDesign d = kicadDesign();
    auto& tp = component(d, "TP1", 5, -5, /*mountType=*/2);
    terminal(tp, "1", 5, -5, 1);
    flash(d, "f.cu", 5, -5, "r1500.0", OdbSymbol::ROUND, 1.5);
    flash(d, "f.mask", 5, -5, "r1600.0", OdbSymbol::ROUND, 1.6);
    d.layerFeatures["f.paste"].units = "MM";  // paste layer present, no opening at TP1
    KicadPcb pcb = transformToKicad(d);
    const KicadPad* p = findPad(pcb, "TP1", "1");
    CHECK(p);
    if (!p) return;
    CHECK_EQ(p->type, std::string("smd"));
    CHECK(hasLayer(*p, "F.Cu") && hasLayer(*p, "F.Mask"));
    CHECK(!hasLayer(*p, "F.Paste"));  // the design has paste layers but no opening here
}

// A slotted TH pad (e.g. a USB-C shell): the drill symbol is an oval, written as
// (drill oval w h) in the pad's frame. A 90-degree flash swaps the slot axes.
TEST(slot_drill_keeps_its_oval) {
    OdbDesign d = kicadDesign();
    auto& j = component(d, "J12", 0, 0, 1);
    terminal(j, "1", 4, -2, 1);
    terminal(j, "2", 8, -2, 1);
    for (const char* cu : {"f.cu", "b.cu"}) {
        flash(d, cu, 4, -2, "oval1200.0x2000.0", OdbSymbol::OVAL, 1.2, 2.0);
        flash(d, cu, 8, -2, "oval1200.0x2000.0", OdbSymbol::OVAL, 1.2, 2.0, {}, 90.0);
    }
    flash(d, "drill_plated_f.cu-b.cu", 4, -2, "oval800.0x1500.0", OdbSymbol::OVAL, 0.8, 1.5, {{".drill", "0"}});
    flash(d, "drill_plated_f.cu-b.cu", 8, -2, "oval800.0x1500.0", OdbSymbol::OVAL, 0.8, 1.5, {{".drill", "0"}}, 90.0);
    KicadPcb pcb = transformToKicad(d);
    const KicadPad* p = findPad(pcb, "J12", "1");
    CHECK(p);
    if (p) {
        CHECK_EQ(p->type, std::string("thru_hole"));
        CHECK_NEAR(p->drill, 0.8, 1e-9);
        CHECK_NEAR(p->drillHeight, 1.5, 1e-9);
    }
    p = findPad(pcb, "J12", "2");
    if (p) {
        CHECK_NEAR(p->drill, 1.5, 1e-9);
        CHECK_NEAR(p->drillHeight, 0.8, 1e-9);
    }
    CHECK(writeString(pcb).find("(drill oval 0.8 1.5)") != std::string::npos);
}

// A custom-shape SMD pad has no copper flash at its anchor (its copper, mask and paste are
// surfaces): the mask/paste openings are found as surfaces covering the anchor.
TEST(custom_pad_anchor_takes_mask_and_paste_from_surfaces) {
    OdbDesign d = kicadDesign();
    auto& j = component(d, "J12", 0, 0, 1);
    terminal(j, "A1B12", 2, -2, 1);
    for (const char* l : {"f.mask", "f.paste"}) {
        OdbSurface srf;
        srf.polarity = "P";
        srf.contours = {square(1.5, -1.5, 2.5, -2.5, false)};
        d.layerFeatures[l].units = "MM";
        d.layerFeatures[l].surfaces.push_back(srf);
    }
    KicadPcb pcb = transformToKicad(d);
    const KicadPad* p = findPad(pcb, "J12", "A1B12");
    CHECK(p);
    if (p) CHECK(hasLayer(*p, "F.Cu") && hasLayer(*p, "F.Mask") && hasLayer(*p, "F.Paste"));
}

// ---------------------------------------------------------------- inner copper layers

static bool hasKicadLayer(const KicadPcb& pcb, int id, const std::string& name) {
    for (auto& l : pcb.layers) if (l.id == id && l.canonicalName == name && l.type == "signal") return true;
    return false;
}

// KiCad names its copper F.CU / IN1.CU / IN2.CU / B.CU. Finding F.Cu and B.Cu by name used to end
// the mapping, so every IN*.CU feature (tracks, planes) was dropped. Read from a real matrix file.
TEST(kicad_inner_layers_are_mapped_from_the_matrix) {
    std::string dir = tempDir("inner_layers");
    writeFile(dir + "/matrix/matrix",
        "STEP {\n   COL=1\n   NAME=PCB\n}\n\n"
        "LAYER {\n   ROW=1\n   CONTEXT=BOARD\n   TYPE=SIGNAL\n   NAME=F.CU\n   POLARITY=POSITIVE\n}\n"
        "LAYER {\n   ROW=2\n   CONTEXT=BOARD\n   TYPE=DIELECTRIC\n   NAME=DIELECTRIC_1\n   POLARITY=POSITIVE\n}\n"
        "LAYER {\n   ROW=3\n   CONTEXT=BOARD\n   TYPE=POWER_GROUND\n   NAME=IN1.CU\n   POLARITY=POSITIVE\n}\n"
        "LAYER {\n   ROW=4\n   CONTEXT=BOARD\n   TYPE=SIGNAL\n   NAME=IN2.CU\n   POLARITY=POSITIVE\n}\n"
        "LAYER {\n   ROW=5\n   CONTEXT=BOARD\n   TYPE=SIGNAL\n   NAME=B.CU\n   POLARITY=POSITIVE\n}\n");
    OdbDesign d;
    CHECK(parseMatrix(dir + "/matrix/matrix", d.stepName, d.layers));
    CHECK_EQ(d.layers.size(), (size_t)5);
    d.edaData.nets = {{"$NONE$"}, {"GND"}};
    auto& in2 = d.layerFeatures["in2.cu"];
    in2.units = "MM";
    int sym = symbol(in2, "r250.0", OdbSymbol::ROUND, 0.25);
    OdbLine ln;
    ln.x1 = 1; ln.y1 = -1; ln.x2 = 5; ln.y2 = -1; ln.symIdx = sym; ln.polarity = "P";
    in2.lines.push_back(ln);
    KicadPcb pcb = transformToKicad(d);
    CHECK(hasKicadLayer(pcb, 0, "F.Cu"));
    CHECK(hasKicadLayer(pcb, 1, "In1.Cu"));
    CHECK(hasKicadLayer(pcb, 2, "In2.Cu"));
    CHECK(hasKicadLayer(pcb, 31, "B.Cu"));
    CHECK_EQ(pcb.segments.size(), (size_t)1);
    if (!pcb.segments.empty()) CHECK_EQ(pcb.segments[0].layer, std::string("In2.Cu"));
}

// Non-KiCad sources (SIGNAL_n naming) keep the row-order mapping: first/last outer, the rest inner.
TEST(row_order_inner_layers_for_generic_names) {
    OdbDesign d;
    d.layers = {layer(1, "SIGNAL", "TOP"), layer(2, "POWER_GROUND", "PLANE_GND"),
                layer(3, "MIXED", "SIG_3"), layer(4, "SIGNAL", "BOTTOM")};
    KicadPcb pcb = transformToKicad(d);
    CHECK(hasKicadLayer(pcb, 0, "F.Cu"));
    CHECK(hasKicadLayer(pcb, 1, "In1.Cu"));
    CHECK(hasKicadLayer(pcb, 2, "In2.Cu"));
    CHECK(hasKicadLayer(pcb, 31, "B.Cu"));
}

// ---------------------------------------------------------------- zone nets from FID records


// A B.Cu GND pour encloses a 3V3 TH pad that sits in a clearance hole of
// the pour. The old reader took the net of the first pad inside the outer outline (3V3).
static OdbDesign gndPourAroundClearance() {
    OdbDesign d = kicadDesign();
    auto& j = component(d, "J1", 0, 0, 2);
    terminal(j, "1", 5, -5, /*3V3*/ 2);    // in the clearance hole
    terminal(j, "2", 15, -15, /*GND*/ 1);  // in the fill
    for (const char* cu : {"f.cu", "b.cu"}) {
        flash(d, cu, 5, -5, "r1700.0", OdbSymbol::ROUND, 1.7);
        flash(d, cu, 15, -15, "r1700.0", OdbSymbol::ROUND, 1.7);
    }
    flash(d, "drill_plated_f.cu-b.cu", 5, -5, "r1000.0", OdbSymbol::ROUND, 1.0, -1, {{".drill", "0"}});
    flash(d, "drill_plated_f.cu-b.cu", 15, -15, "r1000.0", OdbSymbol::ROUND, 1.0, -1, {{".drill", "0"}});
    auto& bcu = d.layerFeatures["b.cu"];
    OdbSurface pour;
    pour.polarity = "P";
    pour.featIdx = (int)bcu.pads.size();  // the S record follows the two pad flashes
    pour.contours = {square(0, 0, 20, -20, false), square(3, -3, 7, -7, true)};
    bcu.surfaces.push_back(pour);
    for (int i = 0; i < (int)bcu.pads.size(); i++) bcu.pads[i].featIdx = i;
    return d;
}

static const KicadZone* zoneOn(const KicadPcb& pcb, const std::string& layer) {
    for (auto& z : pcb.zones) if (z.layer == layer) return &z;
    return nullptr;
}

TEST(gnd_pour_takes_its_net_from_the_fid_record) {
    OdbDesign d = gndPourAroundClearance();
    d.edaData.layers = {"b.cu", "drill_plated_f.cu-b.cu", "f.cu"};
    OdbEdaFid fid;
    fid.netIdx = 1;  // GND
    fid.type = "C";
    fid.layerIdx = 0;
    fid.featIdx = d.layerFeatures["b.cu"].surfaces[0].featIdx;
    d.edaData.fids.push_back(fid);
    KicadPcb pcb = transformToKicad(d);
    const KicadZone* z = zoneOn(pcb, "B.Cu");
    CHECK(z);
    if (z) CHECK_EQ(z->netName, std::string("GND"));
}

// Without FID records the fallback only takes a pad that lies in the fill, not one in a hole.
TEST(fallback_ignores_pads_in_pour_clearance_holes) {
    OdbDesign d = gndPourAroundClearance();
    KicadPcb pcb = transformToKicad(d);
    const KicadZone* z = zoneOn(pcb, "B.Cu");
    CHECK(z);
    if (z) CHECK_EQ(z->netName, std::string("GND"));
}

// Parser side: FID records attach to the preceding NET; feature ordinals count every P/L/A/S/T/B
// record (a text record shifts the surface's index).
TEST(eda_fid_records_and_feature_ordinals_are_parsed) {
    std::string dir = tempDir("fid_records");
    writeFile(dir + "/eda/data",
        "HDR KiCad EDA 9.0.9\nUNITS=MM\nLYR b.cu f.cu\n"
        "NET $NONE$\nSNT TOP T 0 0\nFID C 1 0\n"
        "NET GND\nSNT PLN\nFID C 0 2\nSNT TRC\nFID C 0 0\n"
        "NET 3V3\nSNT TOP T 1 0\nFID C 0 1\n"
        "PKG R0603 1 -1 -1 1 0;\nPIN 1 S -0.5 0 0 E S\n");
    writeFile(dir + "/b.cu/features",
        "UNITS=MM\n$0 r250.0\n"
        "L 0 0 1 0 0 P 0\n"
        "T 1 1 0 P 0 0 0.5 0.5 0.1 'X' 0\n"
        "S P 0\nOB 0 0 I\nOS 1 0\nOS 1 1\nOS 0 0\nOE\nSE\n");
    OdbEdaData eda;
    CHECK(parseEdaData(dir + "/eda/data", eda));
    CHECK_EQ(eda.nets.size(), (size_t)3);
    CHECK_EQ(eda.fids.size(), (size_t)4);
    if (eda.fids.size() == 4) {
        CHECK_EQ(eda.fids[1].netIdx, 1);
        CHECK_EQ(eda.fids[1].layerIdx, 0);
        CHECK_EQ(eda.fids[1].featIdx, 2);
        CHECK_EQ(eda.fids[3].netIdx, 2);
    }
    CHECK_EQ(eda.packages.size(), (size_t)1);
    OdbFeatures f;
    CHECK(parseFeatures(dir + "/b.cu/features", f));
    CHECK_EQ(f.lines.size(), (size_t)1);
    CHECK_EQ(f.surfaces.size(), (size_t)1);
    if (!f.lines.empty()) CHECK_EQ(f.lines[0].featIdx, 0);
    if (!f.surfaces.empty()) CHECK_EQ(f.surfaces[0].featIdx, 2);
}

// ---------------------------------------------------------------- NPTH

// KiCad writes an NPTH as a 0.01 mm copper placeholder (r10) plus a hit on drill_non-plated (.drill
// non_plated). It used to come back as a 0.1 mm roundrect SMD dot, or a PLATED hole on TH-tagged
// parts. Now: np_thru_hole, circle the size of the hole, mask opening from the mask layers.
TEST(npth_hit_makes_a_non_plated_hole_sized_to_the_drill) {
    OdbDesign d = kicadDesign();
    auto& dut = component(d, "DUT1", 0, 0, /*mountType=*/0);
    terminal(dut, "NPTH0", 6, -10, 0);
    auto& h = component(d, "H1", 0, 0, /*mountType=*/2);
    terminal(h, "1", 20, -10, 0);
    for (double x : {6.0, 20.0}) {
        for (const char* cu : {"f.cu", "b.cu"}) flash(d, cu, x, -10, "r10.0", OdbSymbol::ROUND, 0.01);
        flash(d, "drill_non-plated_f.cu-b.cu", x, -10, "r5000.0", OdbSymbol::ROUND, 5.0, -1, {{".drill", "1"}});
    }
    flash(d, "f.mask", 6, -10, "r5000.0", OdbSymbol::ROUND, 5.0);
    flash(d, "b.mask", 6, -10, "r5000.0", OdbSymbol::ROUND, 5.0);
    KicadPcb pcb = transformToKicad(d);
    for (auto [ref, pin] : {std::pair<const char*, const char*>{"DUT1", "NPTH0"}, {"H1", "1"}}) {
        const KicadPad* p = findPad(pcb, ref, pin);
        CHECK(p);
        if (!p) continue;
        CHECK_EQ(p->type, std::string("np_thru_hole"));
        CHECK_EQ(p->shape, std::string("circle"));
        CHECK_NEAR(p->drill, 5.0, 1e-9);
        CHECK_NEAR(p->width, 5.0, 1e-9);
        CHECK_NEAR(p->height, 5.0, 1e-9);
        CHECK(!hasLayer(*p, "F.Paste") && !hasLayer(*p, "B.Paste"));
    }
    const KicadPad* p = findPad(pcb, "DUT1", "NPTH0");
    if (p) CHECK(hasLayer(*p, "F.Mask") && hasLayer(*p, "B.Mask"));
    p = findPad(pcb, "H1", "1");
    if (p) CHECK(!hasLayer(*p, "F.Mask") && !hasLayer(*p, "B.Mask"));
    CHECK(writeString(pcb).find("(pad \"NPTH0\" np_thru_hole circle") != std::string::npos);
}

// Without a .drill attribute the layer name says non-plated (other exporters name it NPTH).
TEST(npth_layer_name_without_drill_attribute) {
    OdbDesign d = kicadDesign();
    d.layers.push_back(layer(13, "DRILL", "NPTH"));
    auto& h = component(d, "H2", 0, 0, 2);
    terminal(h, "1", 3, -3, 0);
    flash(d, "npth", 3, -3, "r3200.0", OdbSymbol::ROUND, 3.2);
    KicadPcb pcb = transformToKicad(d);
    const KicadPad* p = findPad(pcb, "H2", "1");
    CHECK(p);
    if (p) {
        CHECK_EQ(p->type, std::string("np_thru_hole"));
        CHECK_NEAR(p->drill, 3.2, 1e-9);
    }
}

// ---------------------------------------------------------------- donut_rc on Edge.Cuts

// KiCad exports an Edge.Cuts gr_rect as one P record with donut_rc<w>x<h>x<lw>xr<r>: outer box =
// rectangle + stroke. The round-donut branch matched it by prefix and drew a circle of radius w/2
// (a 650 x 72 mm rectangular outline came back as a 646 mm circle).
TEST(donut_rc_on_edge_cuts_is_the_rectangle_it_describes) {
    OdbSymbol rc = decodeSymbol("donut_rc650050.0x72050.0x50.0xr25.0");
    CHECK(rc.donut);
    CHECK_EQ((int)rc.shape, (int)OdbSymbol::ROUNDRECT);
    CHECK_NEAR(rc.lineWidth, 0.05, 1e-9);
    OdbSymbol r = decodeSymbol("donut_r3000.0x2000.0");
    CHECK(r.donut);
    CHECK_EQ((int)r.shape, (int)OdbSymbol::ROUND);
    CHECK_NEAR(r.lineWidth, 0.5, 1e-9);

    OdbDesign d = kicadDesign();
    auto& ec = d.layerFeatures["edge.cuts"];
    ec.units = "MM";
    ec.symbols = {rc, r};
    OdbPad outline;
    outline.x = 325; outline.y = -36; outline.symIdx = 0; outline.mirror = 8;
    OdbPad hole;
    hole.x = 10; hole.y = -10; hole.symIdx = 1; hole.mirror = 8;
    ec.pads = {outline, hole};
    KicadPcb pcb = transformToKicad(d);
    CHECK_EQ(pcb.grRects.size(), (size_t)1);
    if (!pcb.grRects.empty()) {
        auto& g = pcb.grRects[0];
        CHECK_NEAR(g.x1, 0, 1e-9);
        CHECK_NEAR(g.y1, 0, 1e-9);
        CHECK_NEAR(g.x2, 650, 1e-9);
        CHECK_NEAR(g.y2, 72, 1e-9);
        CHECK_NEAR(g.width, 0.05, 1e-9);
        CHECK_EQ(g.layer, std::string("Edge.Cuts"));
    }
    // the round donut is still a circle
    CHECK_EQ(pcb.grCircles.size(), (size_t)1);
}

// KiCad exports an Edge.Cuts circle (a cutout) as donut_r<od>x<id>: the stroke is (od - id) / 2
// wide around radius (od + id) / 4. It was drawn at the outer radius od / 2 with a 0.05 stroke, so
// every cutout grew by half a stroke (35 mm2 of outline area on a board with several cutouts).
TEST(edge_cuts_donut_circle_on_its_centreline) {
    OdbDesign d = kicadDesign();
    auto& ec = d.layerFeatures["edge.cuts"];
    ec.units = "MM";
    ec.symbols = {decodeSymbol("donut_r28700.0x28550.0")};
    OdbPad hole;
    hole.x = 56.7; hole.y = -148.5; hole.symIdx = 0; hole.mirror = 8;
    ec.pads = {hole};
    KicadPcb pcb = transformToKicad(d);
    CHECK_EQ(pcb.grCircles.size(), (size_t)1);
    CHECK_EQ(pcb.grArcs.size(), (size_t)0);
    if (!pcb.grCircles.empty()) {
        auto& c = pcb.grCircles[0];
        CHECK_NEAR(c.cx, 56.7, 1e-9);
        CHECK_NEAR(c.cy, 148.5, 1e-9);
        CHECK_NEAR(c.r, 14.3125, 1e-9);
        CHECK_NEAR(c.width, 0.075, 1e-9);
        CHECK(!c.filled);
        CHECK_EQ(c.layer, std::string("Edge.Cuts"));
    }
}

// KiCad 9 writes a circle (R, stroke w) as donut_r(2R + w)x(2R - w/2): here R 14.3, w 0.1
static OdbDesign edgeCircleFrom(const std::string& saveApp) {
    OdbDesign d = kicadDesign();
    d.info.odbSource = "KiCad EDA";
    d.info.saveApp = saveApp;
    auto& ec = d.layerFeatures["edge.cuts"];
    ec.units = "MM";
    ec.symbols = {decodeSymbol("donut_r28700.0x28550.0")};
    OdbPad hole;
    hole.x = 56.7; hole.y = -148.5; hole.symIdx = 0; hole.mirror = 8;
    ec.pads = {hole};
    applyProducerQuirks(d);
    return d;
}

TEST(kicad9_donut_circle_gets_its_radius_and_stroke_back) {
    KicadPcb pcb = transformToKicad(edgeCircleFrom("KiCad EDA 9.0.9"));
    CHECK_EQ(pcb.grCircles.size(), (size_t)1);
    if (!pcb.grCircles.empty()) {
        CHECK_NEAR(pcb.grCircles[0].r, 14.3, 1e-9);
        CHECK_NEAR(pcb.grCircles[0].width, 0.1, 1e-9);
    }
}

TEST(donut_circle_from_other_producers_is_read_per_spec) {
    for (const char* app : {"KiCad EDA 10.0.0", "", "Some CAD 9.0"}) {
        KicadPcb pcb = transformToKicad(edgeCircleFrom(app));
        CHECK_EQ(pcb.grCircles.size(), (size_t)1);
        if (!pcb.grCircles.empty()) {
            CHECK_NEAR(pcb.grCircles[0].r, 14.3125, 1e-9);
            CHECK_NEAR(pcb.grCircles[0].width, 0.075, 1e-9);
        }
    }
}

TEST(save_app_is_parsed_from_misc_info) {
    std::string dir = tempDir("misc_info");
    writeFile(dir + "/info", "UNITS=MM\nODB_SOURCE=KiCad EDA\nSAVE_APP=KiCad EDA 9.0.9\n");
    OdbInfo info;
    CHECK(parseInfo(dir + "/info", info));
    CHECK_EQ(info.saveApp, std::string("KiCad EDA 9.0.9"));
    CHECK(hasKicadDonutSkew(info));
    info.saveApp = "KiCad EDA 8.99.0-1234";
    CHECK(hasKicadDonutSkew(info));
    info.saveApp = "KiCad EDA 10.0.1";
    CHECK(!hasKicadDonutSkew(info));
}

// ---------------------------------------------------------------- DNP footprints

// kicad-cli 9 writes a footprint's DNP flag as the boolean .no_pop attribute on its CMP record
// (an attribute index with no '='). The attribute is only defined when some footprint is DNP.
TEST(no_pop_attribute_is_parsed_from_cmp_records) {
    std::string dir = tempDir("no_pop");
    writeFile(dir + "/components",
              "UNITS=MM\n@0 .no_pop\n@1 .comp_mount_type\n"
              "CMP 0 1.0 -1.0 0 N R1 R_0805 ;0,1=1\n"
              "CMP 0 3.0 -1.0 0 N R2 R_0805 ;1=1\n");
    OdbComponents comps;
    CHECK(parseComponents(dir + "/components", comps));
    CHECK_EQ(comps.components.size(), (size_t)2);
    if (comps.components.size() == 2) {
        CHECK(comps.components[0].noPop);
        CHECK_EQ(comps.components[0].mountType, 1);
        CHECK(!comps.components[1].noPop);
        CHECK_EQ(comps.components[1].mountType, 1);
    }
}

// Either source marks the footprint dnp: the .no_pop flag, or a 'Config' field whose whole value or
// one comma/semicolon-separated token spells out DNP. Neither clears the other.
TEST(dnp_comes_from_no_pop_or_a_config_token) {
    OdbDesign d = kicadDesign();
    component(d, "R1", 0, 0, 1).noPop = true;
    component(d, "R2", 2, 0, 1).properties["Config"] = "Rev B; DNP";
    component(d, "R3", 4, 0, 1).properties["Config"] = "Do Not Populate";
    component(d, "R4", 6, 0, 1).properties["Config"] = "DNP-ready";
    component(d, "R5", 8, 0, 1).properties["Config"] = "Rev B";
    auto& r6 = component(d, "R6", 10, 0, 1);
    r6.noPop = true;
    r6.properties["Config"] = "Fitted";
    component(d, "R7", 12, 0, 1);
    KicadPcb pcb = transformToKicad(d);
    std::map<std::string, bool> dnp;
    for (auto& fp : pcb.footprints) dnp[fp.refdes] = fp.dnp;
    CHECK(dnp["R1"]);
    CHECK(dnp["R2"]);
    CHECK(dnp["R3"]);
    CHECK(!dnp["R4"]);
    CHECK(!dnp["R5"]);
    CHECK(dnp["R6"]);
    CHECK(!dnp["R7"]);
    std::string out = writeString(pcb);
    size_t n = 0;
    for (size_t pos = 0; (pos = out.find("(attr smd dnp)", pos)) != std::string::npos; ++pos) n++;
    CHECK_EQ(n, (size_t)4);
}

// ---------------------------------------------------------------- outline written once

static OdbDesign boardWithProfile() {
    OdbDesign d = kicadDesign();
    OdbSurface s;
    s.polarity = "P";
    s.contours = {square(0, 0, 50, -30, false)};
    d.profile.surfaces.push_back(s);
    return d;
}

// KiCad exports the outline twice: the step profile and the Edge.Cuts layer. Both were written,
// so overlapping duplicate segments broke KiCad's board outline (DRC 'malformed', zone fill, 3D).
TEST(profile_is_skipped_when_edge_cuts_has_features) {
    OdbDesign d = boardWithProfile();
    auto& ec = d.layerFeatures["edge.cuts"];
    ec.units = "MM";
    int sym = symbol(ec, "r50.0", OdbSymbol::ROUND, 0.05);
    double xy[5][2] = {{0, 0}, {50, 0}, {50, -30}, {0, -30}, {0, 0}};
    for (int i = 0; i < 4; i++) {
        OdbLine l;
        l.x1 = xy[i][0]; l.y1 = xy[i][1]; l.x2 = xy[i + 1][0]; l.y2 = xy[i + 1][1];
        l.symIdx = sym;
        ec.lines.push_back(l);
    }
    KicadPcb pcb = transformToKicad(d);
    int edge = 0;
    for (auto& g : pcb.grLines) edge += g.layer == "Edge.Cuts";
    for (auto& g : pcb.grRects) edge += g.layer == "Edge.Cuts";
    CHECK_EQ(edge, 4);
    CHECK_EQ(pcb.grRects.size(), (size_t)0);
}

// A design whose outline exists only as the profile still gets it.
TEST(profile_is_the_outline_when_there_is_no_edge_layer_geometry) {
    OdbDesign d = boardWithProfile();
    d.layerFeatures["edge.cuts"].units = "MM";  // layer present, empty
    KicadPcb pcb = transformToKicad(d);
    CHECK_EQ(pcb.grRects.size(), (size_t)1);
    if (!pcb.grRects.empty()) {
        CHECK_NEAR(pcb.grRects[0].x2 - pcb.grRects[0].x1, 50, 1e-9);
        CHECK_NEAR(pcb.grRects[0].y2 - pcb.grRects[0].y1, 30, 1e-9);
    }
}

// ---------------------------------------------------------------- rotation

// Board position of a pad the way KiCad places it: pos + (x cos + y sin, -x sin + y cos)
static std::pair<double, double> kicadAbs(const KicadFootprint& fp, const KicadPad& p) {
    double c = std::cos(fp.angle * M_PI / 180), s = std::sin(fp.angle * M_PI / 180);
    return {fp.x + p.x * c + p.y * s, fp.y - p.x * s + p.y * c};
}

static const KicadFootprint* findFp(const KicadPcb& pcb, const std::string& ref) {
    for (auto& fp : pcb.footprints) if (fp.refdes == ref) return &fp;
    return nullptr;
}

// Footprints were written at 0 deg with pads at board offsets, and a rotated rect pad kept its
// unrotated size (a 0.8 x 0.95 pad at 270 deg came back at 0 deg). Top side: CMP/TOP and the
// P record (orient 8) are clockwise.
TEST(top_rotation_is_carried_to_footprint_and_pads) {
    OdbDesign d = kicadDesign();
    auto& r1 = component(d, "R1", 10, -20, 1, false, /*rotation=*/90.0);
    terminal(r1, "1", 10, -19.2, 1, 90.0);
    terminal(r1, "2", 10, -20.8, 2, 90.0);
    flash(d, "f.cu", 10, -19.2, "rect800.0x950.0xr200.0", OdbSymbol::ROUNDRECT, 0.8, 0.95, {}, 90.0, 8);
    flash(d, "f.cu", 10, -20.8, "rect800.0x950.0xr200.0", OdbSymbol::ROUNDRECT, 0.8, 0.95, {}, 90.0, 8);
    KicadPcb pcb = transformToKicad(d);
    const KicadFootprint* fp = findFp(pcb, "R1");
    CHECK(fp);
    if (!fp) return;
    CHECK_NEAR(fp->angle, 270.0, 1e-9);
    for (auto& p : fp->pads) {
        CHECK_NEAR(p.angle, 270.0, 1e-9);
        CHECK_NEAR(p.width, 0.8, 1e-9);
        CHECK_NEAR(p.height, 0.95, 1e-9);
    }
    const KicadPad* p1 = findPad(pcb, "R1", "1");
    if (p1) {
        auto [x, y] = kicadAbs(*fp, *p1);
        CHECK_NEAR(x, 10.0, 1e-9);
        CHECK_NEAR(y, 19.2, 1e-9);
        CHECK_NEAR(p1->x, -0.8, 1e-9);  // the library pad 1 position of a 0603 at 0 deg
        CHECK_NEAR(p1->y, 0.0, 1e-9);
    }
    std::string out = writeString(pcb);
    CHECK(out.find("(at 10 20 270)") != std::string::npos);
    CHECK(out.find("(pad \"1\" smd roundrect (at -0.8 0 270)") != std::string::npos);
}

// Bottom side, as kicad-cli 9.0.9 exports a flipped footprint at 30 deg: CMP 330 M, TOP 330 M,
// copper flash orient 9 with angle 30 (the mirrored form carries KiCad's CCW angle).
TEST(bottom_rotation_matches_kicad_export) {
    OdbDesign d = kicadDesign();
    auto& j5 = component(d, "J5", 140.47, -97.294999, 1, true, /*rotation=*/330.0);
    terminal(j5, "1", 131.333534, -99.250175, 1, 330.0);
    flash(d, "b.cu", 131.333534, -99.250175, "rect1244.60x4749.80", OdbSymbol::RECT, 1.2446, 4.7498, {}, 30.0, 9);
    d.info.odbSource = "KiCad EDA";
    applyProducerQuirks(d);
    KicadPcb pcb = transformToKicad(d);
    const KicadFootprint* fp = findFp(pcb, "J5");
    const KicadPad* p = findPad(pcb, "J5", "1");
    CHECK(fp && p);
    if (!fp || !p) return;
    CHECK_EQ(fp->layer, std::string("B.Cu"));
    CHECK_NEAR(fp->angle, 30.0, 1e-9);
    CHECK_NEAR(p->angle, 30.0, 1e-9);
    auto [x, y] = kicadAbs(*fp, *p);
    CHECK_NEAR(x, 131.333534, 1e-6);
    CHECK_NEAR(y, 99.250175, 1e-6);
    CHECK(hasLayer(*p, "B.Cu"));
}

// ---------------------------------------------------------------- no synthesized ref/value text

// The ODB++ carries the visible reference/value text as stroked lines; the converter added a second,
// visible fp_text copy at (0 -1.65) F.SilkS / (0 1.65) F.Fab. The fields are now hidden properties
// (bottom parts on B.SilkS / B.Fab, mirrored), with no fp_text.
TEST(reference_and_value_fields_are_hidden) {
    OdbDesign d = kicadDesign();
    auto& r = component(d, "R7", 1, -1, 1);
    r.properties["Value"] = "10k";
    terminal(r, "1", 1, -1, 1);
    component(d, "U9", 5, -5, 1, true);
    std::string out = writeString(transformToKicad(d));
    CHECK(out.find("fp_text") == std::string::npos);
    CHECK(out.find("(property \"Reference\" \"R7\" (at 0 -1.65) (layer \"F.SilkS\") hide") != std::string::npos);
    CHECK(out.find("(property \"Value\" \"10k\" (at 0 1.65) (layer \"F.Fab\") hide") != std::string::npos);
    CHECK(out.find("(property \"Reference\" \"U9\" (at 0 -1.65) (layer \"B.SilkS\") hide") != std::string::npos);
    CHECK(out.find("(justify mirror)") != std::string::npos);
}

// ---------------------------------------------------------------- non-copper pads and surfaces

// Only lines and arcs were converted on non-copper layers: filled silk shapes and glyphs
// (surfaces), graphic mask openings and the donut markers on user/fab layers (flashes) vanished.
TEST(non_copper_flashes_and_surfaces_become_graphics) {
    OdbDesign d = kicadDesign();
    d.layers.push_back(layer(14, "DOCUMENT", "USER.DRAWINGS"));
    d.layers.push_back(layer(15, "SILK_SCREEN", "F.SILKSCREEN"));
    auto& r = component(d, "R1", 0, 0, 1);
    terminal(r, "1", 1, -1, 1);
    flash(d, "f.cu", 1, -1, "rect500.0x600.0", OdbSymbol::RECT, 0.5, 0.6);
    flash(d, "f.mask", 1, -1, "rect600.0x700.0", OdbSymbol::RECT, 0.6, 0.7);  // R1.1's own opening
    flash(d, "f.mask", 20, -20, "r3000.0", OdbSymbol::ROUND, 3.0);            // graphic opening
    auto& user = d.layerFeatures["user.drawings"];
    user.units = "MM";
    user.symbols.push_back(decodeSymbol("donut_r2000.0x1800.0"));
    OdbPad marker;
    marker.x = 5; marker.y = -5; marker.symIdx = 0; marker.mirror = 8;
    user.pads.push_back(marker);
    auto& silk = d.layerFeatures["f.silkscreen"];
    silk.units = "MM";
    OdbSurface logo;
    logo.polarity = "P";
    logo.contours = {square(10, -10, 14, -14, false), square(11, -11, 13, -13, true)};
    silk.surfaces.push_back(logo);

    KicadPcb pcb = transformToKicad(d);
    int maskCircles = 0, userRings = 0;
    for (auto& c : pcb.grCircles) {
        if (c.layer == "F.Mask") { maskCircles++; CHECK(c.filled); CHECK_NEAR(c.r, 1.5, 1e-9); }
        if (c.layer == "Dwgs.User") { userRings++; CHECK(!c.filled); CHECK_NEAR(c.r, 0.95, 1e-9); CHECK_NEAR(c.width, 0.1, 1e-9); }
    }
    CHECK_EQ(maskCircles, 1);
    CHECK_EQ(userRings, 1);
    int maskPolys = 0;
    for (auto& p : pcb.grPolys) maskPolys += p.layer == "F.Mask";
    CHECK_EQ(maskPolys, 0);  // the pad's own opening is not duplicated
    const KicadGrPoly* logoPoly = nullptr;
    for (auto& p : pcb.grPolys) if (p.layer == "F.SilkS") logoPoly = &p;
    CHECK(logoPoly);
    if (logoPoly) {
        // 4 outer + 4 hole vertices + the hole entry repeated + the outer splice point repeated
        CHECK_EQ(logoPoly->pts.size(), (size_t)10);
        double area2 = 0;  // shoelace: keyholed polygon area = 16 - 4
        for (size_t i = 0; i < logoPoly->pts.size(); i++) {
            auto& a = logoPoly->pts[i];
            auto& b = logoPoly->pts[(i + 1) % logoPoly->pts.size()];
            area2 += a.first * b.second - b.first * a.second;
        }
        CHECK_NEAR(std::abs(area2) / 2, 12.0, 1e-9);
    }
    std::string out = writeString(pcb);
    CHECK(out.find("(gr_poly") != std::string::npos);
    CHECK(out.find("(fill solid) (layer \"F.SilkS\")") != std::string::npos);
    CHECK(out.find("(gr_circle (center 5 5) (end 5.95 5)") != std::string::npos);
}

// ---------------------------------------------------------------- custom-shape pads

// KiCad exports a custom-shape pad as a copper surface owned by the pin (SNT TOP T <comp> <pin>),
// with no flash at the anchor. It came back as a 0.1 mm roundrect pad plus a separate zone.
TEST(pin_surface_becomes_the_custom_pad_shape) {
    OdbDesign d = kicadDesign();
    auto& j = component(d, "J12", 0, 0, 1);
    terminal(j, "A5", 5, -5, 2);       // ordinary pad, pin 0
    terminal(j, "A1B12", 2, -2, 1);    // custom pad, pin 1
    flash(d, "f.cu", 5, -5, "rect300.0x1300.0", OdbSymbol::RECT, 0.3, 1.3);
    auto& fcu = d.layerFeatures["f.cu"];
    fcu.pads[0].featIdx = 0;
    OdbSurface shape;
    shape.polarity = "P";
    shape.featIdx = 1;
    shape.contours = {square(1.7, -1.35, 2.3, -2.65, false)};
    fcu.surfaces.push_back(shape);
    d.edaData.layers = {"f.cu"};
    OdbEdaFid fid;
    fid.netIdx = 1;
    fid.sntType = "TOP";
    fid.sntSide = "T";
    fid.sntComp = 0;
    fid.sntToeprint = 1;
    fid.type = "C";
    fid.layerIdx = 0;
    fid.featIdx = 1;
    d.edaData.fids.push_back(fid);
    KicadPcb pcb = transformToKicad(d);
    CHECK_EQ(pcb.zones.size(), (size_t)0);
    const KicadPad* p = findPad(pcb, "J12", "A1B12");
    CHECK(p);
    if (!p) return;
    CHECK_EQ(p->shape, std::string("custom"));
    CHECK_EQ(p->primitives.size(), (size_t)1);
    if (!p->primitives.empty()) {
        CHECK_EQ(p->primitives[0].size(), (size_t)4);
        double area2 = 0;
        auto& q = p->primitives[0];
        for (size_t i = 0; i < q.size(); i++)
            area2 += q[i].first * q[(i + 1) % q.size()].second - q[(i + 1) % q.size()].first * q[i].second;
        CHECK_NEAR(std::abs(area2) / 2, 0.6 * 1.3, 1e-9);
        for (auto& [x, y] : q) CHECK(std::abs(x) <= 0.3 + 1e-9 && std::abs(y) <= 0.65 + 1e-9);  // pad-relative
    }
    CHECK(hasLayer(*p, "F.Cu"));
    const KicadPad* a5 = findPad(pcb, "J12", "A5");
    if (a5) CHECK_EQ(a5->shape, std::string("rect"));
    std::string out = writeString(pcb);
    CHECK(out.find("(pad \"A1B12\" smd custom") != std::string::npos);
    CHECK(out.find("(options (clearance outline) (anchor circle))") != std::string::npos);
}

TEST(snt_top_records_are_parsed) {
    std::string dir = tempDir("snt_top");
    writeFile(dir + "/eda/data",
        "UNITS=MM\nLYR f.cu\nNET $NONE$\nNET GND\nSNT TOP T 3 7\nFID C 0 12\nSNT PLN\nFID C 0 13\n");
    OdbEdaData eda;
    CHECK(parseEdaData(dir + "/eda/data", eda));
    CHECK_EQ(eda.fids.size(), (size_t)2);
    if (eda.fids.size() == 2) {
        CHECK_EQ(eda.fids[0].sntType, std::string("TOP"));
        CHECK_EQ(eda.fids[0].sntSide, std::string("T"));
        CHECK_EQ(eda.fids[0].sntComp, 3);
        CHECK_EQ(eda.fids[0].sntToeprint, 7);
        CHECK_EQ(eda.fids[1].sntType, std::string("PLN"));
        CHECK(eda.fids[0].sntIdx != eda.fids[1].sntIdx);
    }
}

// ---------------------------------------------------------------- copper graphics are not zones

// Every copper surface became a zone, so copper logos and drawn copper polygons came back as netless
// zones (the first refill changes them). With an FID map, a surface the map does not mention is
// such a graphic and is written as a filled gr_poly on its copper layer; mapped ones stay zones.
TEST(copper_surface_without_fid_is_a_graphic) {
    OdbDesign d = gndPourAroundClearance();  // B.Cu pour, feature 2
    d.edaData.layers = {"b.cu"};
    OdbEdaFid fid;
    fid.netIdx = 1;
    fid.sntType = "PLN";
    fid.type = "C";
    fid.layerIdx = 0;
    fid.featIdx = d.layerFeatures["b.cu"].surfaces[0].featIdx;
    d.edaData.fids.push_back(fid);
    OdbSurface logo;
    logo.polarity = "P";
    logo.featIdx = 7;
    logo.contours = {square(30, -30, 32, -32, false)};
    d.layerFeatures["f.cu"].surfaces.push_back(logo);
    KicadPcb pcb = transformToKicad(d);
    CHECK_EQ(pcb.zones.size(), (size_t)1);
    if (!pcb.zones.empty()) CHECK_EQ(pcb.zones[0].netName, std::string("GND"));
    int cuPolys = 0;
    for (auto& p : pcb.grPolys) cuPolys += p.layer == "F.Cu";
    CHECK_EQ(cuPolys, 1);
}

// No FID map (non-KiCad exporters without eda/data nets): surfaces stay zones as before.
TEST(without_fids_copper_surfaces_stay_zones) {
    OdbDesign d = gndPourAroundClearance();
    KicadPcb pcb = transformToKicad(d);
    CHECK_EQ(pcb.zones.size(), (size_t)1);
    CHECK_EQ(pcb.grPolys.size(), (size_t)0);
}

// ---------------------------------------------------------------- main

// ---------------------------------------------------------------- package outlines -> courtyards

// The RC/CR/SQ/CT records between PKG and its first PIN are the package outline (KiCad writes the
// footprint courtyard there). They were skipped. Arcs (OC) are flattened in steps of <= 15 deg;
// outline records after a PIN are the pin's own and stay out.
TEST(package_outline_records_are_parsed) {
    std::string dir = tempDir("pkg_outline");
    writeFile(dir + "/eda/data",
        "HDR KiCad EDA 9.0.9\nUNITS=MM\nLYR f.cu\n"
        "PKG SOT23 0.95 -2 -1.5 2 1.5;\n"
        "CT\nOB -1 -1 I\nOS 1 -1\nOC 1 1 1 0 N\nOS -1 1\nOS -1 -1\nOE\n"
        "OB -0.2 -0.2 H\nOS 0.2 -0.2\nOS 0.2 0.2\nOE\nCE\n"
        "RC 2 2 1 0.5\nSQ 0 5 0.5\nCR 0 -5 1\n"
        "PIN 1 S -0.95 -1 0 E S\nRC -1 -1.2 0.1 0.1\n"
        "PKG R0603 1.6 -1.5 -0.7 1.5 0.7;\nPIN 1 S -0.8 0 0 E S\nCT\nOB -1 -0.3 I\nOS -0.6 -0.3\nOS -0.6 0.3\nOE\nCE\n");
    OdbEdaData eda;
    CHECK(parseEdaData(dir + "/eda/data", eda));
    CHECK_EQ(eda.packages.size(), (size_t)2);
    if (eda.packages.size() != 2) return;
    auto& o = eda.packages[0].outlines;
    CHECK_EQ(o.size(), (size_t)4);  // contour island (hole dropped), RC, SQ, CR
    CHECK(eda.packages[1].outlines.empty());
    CHECK_EQ(eda.packages[0].pins.size(), (size_t)1);
    if (o.size() != 4) return;
    // island: (-1,-1) (1,-1), 180 deg arc around (1,0) in 12 steps -> 11 inner points + (1,1), (-1,1)
    CHECK_EQ(o[0].size(), (size_t)15);
    double maxX = -1e9;
    for (auto& [x, y] : o[0]) { maxX = std::max(maxX, x); CHECK_NEAR(x > 1 ? std::hypot(x - 1, y) : 1, 1, 1e-9); }
    CHECK_NEAR(maxX, 2, 1e-9);  // counter-clockwise from (1,-1) bulges to +x
    CHECK_EQ(o[1].size(), (size_t)4);
    CHECK_NEAR(o[1][2].first, 3, 1e-9);
    CHECK_NEAR(o[1][2].second, 2.5, 1e-9);
    CHECK_NEAR(o[2][0].first, -0.5, 1e-9);
    CHECK_NEAR(o[2][0].second, 4.5, 1e-9);
    CHECK_EQ(o[3].size(), (size_t)24);
    for (auto& [x, y] : o[3]) CHECK_NEAR(std::hypot(x, y + 5), 1, 1e-9);
}

static void courtyardLine(OdbDesign& d, const std::string& layerName, double x1, double y1, double x2, double y2) {
    auto& f = d.layerFeatures[layerName];
    f.units = "MM";
    if (f.symbols.empty()) symbol(f, "r50.0", OdbSymbol::ROUND, 0.05);
    OdbLine l;
    l.x1 = x1; l.y1 = y1; l.x2 = x2; l.y2 = y2;
    l.symIdx = 0;
    l.polarity = "P";
    f.lines.push_back(l);
}

static OdbDesign courtyardDesign() {
    OdbDesign d = kicadDesign();
    d.layers.push_back(layer(13, "DOCUMENT", "F.COURTYARD"));
    d.layers.push_back(layer(14, "DOCUMENT", "B.COURTYARD"));
    OdbEdaPackage pkg;
    pkg.name = "R0603";
    pkg.outlines = {{{-1.48, -0.73}, {1.48, -0.73}, {1.48, 0.73}, {-1.48, 0.73}}};
    d.edaData.packages.push_back(pkg);
    return d;
}

// kicad-cli writes each courtyard twice: as strokes on F/B.Courtyard (read as board graphics) and as
// the package outline. The outline now claims the strokes it runs along: they move into the
// footprint, on the footprint's side, so KiCad's DRC sees a courtyard per footprint. Top part
// rotated 90 deg (clockwise in ODB++): the outline is 0.73 x 1.48 on the board.
TEST(package_outline_claims_its_courtyard_strokes) {
    OdbDesign d = courtyardDesign();
    auto& r = component(d, "R1", 10, -20, 1, false, 90.0);
    r.pkgIdx = 0;
    courtyardLine(d, "f.courtyard", 9.27, -18.52, 10.73, -18.52);
    courtyardLine(d, "f.courtyard", 10.73, -18.52, 10.73, -21.48);
    courtyardLine(d, "f.courtyard", 10.73, -21.48, 9.27, -21.48);
    courtyardLine(d, "f.courtyard", 9.27, -21.48, 9.27, -18.52);
    courtyardLine(d, "f.courtyard", 30, -30, 31, -30);  // a board-level courtyard stroke stays one
    KicadPcb pcb = transformToKicad(d);
    const KicadFootprint* fp = findFp(pcb, "R1");
    CHECK(fp);
    if (!fp) return;
    CHECK_EQ(fp->fpLines.size(), (size_t)4);
    for (auto& l : fp->fpLines) {
        CHECK_EQ(l.layer, std::string("F.CrtYd"));
        CHECK_NEAR(l.width, 0.05, 1e-9);
        // footprint frame: the unrotated 0603 courtyard
        CHECK_NEAR(std::abs(l.x1), 1.48, 1e-6);
        CHECK_NEAR(std::abs(l.y1), 0.73, 1e-6);
    }
    CHECK_EQ(pcb.grLines.size(), (size_t)1);
    std::string out = writeString(pcb);
    CHECK(out.find("(fp_line (start") != std::string::npos);
    CHECK(out.find("(layer \"F.CrtYd\"))") != std::string::npos);
}

// A footprint without a courtyard gets its bounding hull as the package outline: it runs along no
// courtyard stroke, so nothing is claimed (a hull is not a courtyard). A stroke that covers only
// part of the outline is not enough either.
TEST(hull_outline_without_strokes_claims_nothing) {
    OdbDesign d = courtyardDesign();
    auto& r = component(d, "R1", 0, 0, 1);
    r.pkgIdx = 0;
    courtyardLine(d, "f.courtyard", -1.48, -0.73, 1.48, -0.73);  // one side only: 30% of the ring
    KicadPcb pcb = transformToKicad(d);
    const KicadFootprint* fp = findFp(pcb, "R1");
    CHECK(fp && fp->fpLines.empty());
    CHECK_EQ(pcb.grLines.size(), (size_t)1);
}

// A bottom part (mirror M) claims B.Courtyard strokes. kicad-cli writes bottom packages already
// flipped, so the M flag must not mirror them again; the package pin on the part's terminal says
// so. KiCad writes the courtyard rectangle as a donut_rc flash; it comes back as four fp_lines on
// B.CrtYd in the flipped footprint's frame. Without pins to decide, kicad-cli's placement is used.
static OdbDesign bottomCourtyardDesign(bool withPin) {
    OdbDesign d = courtyardDesign();
    d.edaData.packages[0].outlines = {{{-2.5, -1.0}, {1.5, -1.0}, {1.5, 1.0}, {-2.5, 1.0}}};  // off-centre
    d.edaData.packages[0].pins.push_back({"1", "S", -2.0, 0.0, 0, "E", "S"});
    auto& u = component(d, "U2", 50, -40, 1, true, 0.0);
    u.pkgIdx = 0;
    if (withPin) terminal(u, "1", 48, -40, 1);  // package (-2, 0) as written: not mirrored
    // the courtyard strokes sit at package x -2.5..1.5 -> board 47.5..51.5; centreline 4 x 2
    auto& f = d.layerFeatures["b.courtyard"];
    f.units = "MM";
    f.symbols.push_back(decodeSymbol("donut_rc4050.0x2050.0x50.0xr25.0"));
    OdbPad p;
    p.x = 49.5; p.y = -40; p.symIdx = 0; p.mirror = 0; p.polarity = "P";
    f.pads.push_back(p);
    return d;
}

TEST(bottom_package_outline_claims_a_donut_rc_courtyard) {
    for (bool withPin : {true, false}) {
        OdbDesign d = bottomCourtyardDesign(withPin);
        KicadPcb pcb = transformToKicad(d);
        const KicadFootprint* fp = findFp(pcb, "U2");
        CHECK(fp);
        if (!fp) return;
        CHECK_EQ(fp->layer, std::string("B.Cu"));
        CHECK_EQ(fp->fpLines.size(), (size_t)4);
        double minX = 1e9, maxX = -1e9;
        for (auto& l : fp->fpLines) {
            CHECK_EQ(l.layer, std::string("B.CrtYd"));
            minX = std::min({minX, l.x1, l.x2});
            maxX = std::max({maxX, l.x1, l.x2});
        }
        CHECK_NEAR(minX, -2.5, 1e-6);  // board offset from the footprint origin
        CHECK_NEAR(maxX, 1.5, 1e-6);
        CHECK(pcb.grRects.empty() && pcb.grLines.empty());
    }
}

// The same part from a producer that follows the spec (package written unflipped, M mirrors it):
// the pin lands on the terminal only when mirrored, so the mirrored outline claims the strokes.
TEST(spec_mirrored_bottom_package_is_placed_mirrored) {
    OdbDesign d = bottomCourtyardDesign(false);
    d.edaData.packages[0].outlines = {{{-1.5, -1.0}, {2.5, -1.0}, {2.5, 1.0}, {-1.5, 1.0}}};
    d.edaData.packages[0].pins[0].x = 2.0;
    terminal(d.botComponents.components[0], "1", 48, -40, 1);
    KicadPcb pcb = transformToKicad(d);
    const KicadFootprint* fp = findFp(pcb, "U2");
    CHECK(fp && fp->fpLines.size() == 4);
}

// ---------------------------------------------------------------- round-up bug classes, part 1

static std::vector<const KicadFootprint*> freePads(const KicadPcb& pcb) {
    std::vector<const KicadFootprint*> out;
    for (auto& fp : pcb.footprints)
        if (fp.library == "odb2kicad:FreePad") out.push_back(&fp);
    return out;
}

// Class 1: copper flashes that are no component pin and no via still reach the board, as one
// board-only footprint per site: a plated hit makes it through-hole, a lone non-plated hit an
// NPTH, an outer flash an SMD pad (fiducial named FID), all at their own position and angle.
TEST(free_copper_pads_become_board_only_footprints) {
    OdbDesign d = kicadDesign();
    auto& r1 = component(d, "R1", 50, -50, 1);
    terminal(r1, "1", 50, -50, 1);
    flash(d, "f.cu", 50, -50, "r1000.0", OdbSymbol::ROUND, 1.0);           // the pin: not free
    flash(d, "f.cu", 10, -10, "r1000.0", OdbSymbol::ROUND, 1.0, -1, {{".pad_usage", "2"}});  // fiducial
    flash(d, "b.cu", 20, -10, "rect2000.0x1000.0", OdbSymbol::RECT, 2.0, 1.0, {}, 30);   // bottom test pad
    flash(d, "f.cu", 30, -10, "r1600.0", OdbSymbol::ROUND, 1.6);           // plated mounting hole
    flash(d, "b.cu", 30, -10, "r1600.0", OdbSymbol::ROUND, 1.6);
    flash(d, "drill_plated_f.cu-b.cu", 30, -10, "r800.0", OdbSymbol::ROUND, 0.8, -1, {{".drill", "0"}});
    flash(d, "drill_non-plated_f.cu-b.cu", 40, -10, "r3000.0", OdbSymbol::ROUND, 3.0);

    KicadPcb pcb = transformToKicad(d);
    auto fps = freePads(pcb);
    CHECK_EQ(fps.size(), (size_t)4);
    auto at = [&](double x, double y) -> const KicadFootprint* {
        for (auto* fp : fps) if (std::abs(fp->x - x) < 1e-6 && std::abs(fp->y - y) < 1e-6) return fp;
        return nullptr;
    };
    const KicadFootprint* fid = at(10, 10);
    const KicadFootprint* tp = at(20, 10);
    const KicadFootprint* mh = at(30, 10);
    const KicadFootprint* np = at(40, 10);
    CHECK(fid && tp && mh && np);
    if (!fid || !tp || !mh || !np) return;
    CHECK_EQ(fid->refdes.substr(0, 3), std::string("FID"));
    CHECK_EQ(fid->pads[0].type, std::string("smd"));
    CHECK_EQ(fid->pads[0].shape, std::string("circle"));
    CHECK(hasLayer(fid->pads[0], "F.Cu"));
    CHECK(fid->attr.find("board_only") != std::string::npos);
    CHECK(hasLayer(tp->pads[0], "B.Cu") && !hasLayer(tp->pads[0], "F.Cu"));
    CHECK_NEAR(tp->pads[0].width, 2.0, 1e-9);
    CHECK_NEAR(tp->pads[0].angle, 330.0, 1e-9);   // orient 8, 30 deg clockwise
    CHECK_EQ(mh->pads[0].type, std::string("thru_hole"));
    CHECK_NEAR(mh->pads[0].drill, 0.8, 1e-9);
    CHECK_NEAR(mh->pads[0].width, 1.6, 1e-9);
    CHECK_EQ(np->pads[0].type, std::string("np_thru_hole"));
    CHECK_NEAR(np->pads[0].width, 3.0, 1e-9);
    // R1's own pad is not duplicated as a free pad
    CHECK(!at(50, 50));
    std::string out = writeString(pcb);
    CHECK(out.find("(attr smd board_only exclude_from_pos_files exclude_from_bom)") != std::string::npos);
}

// Class 2: OC records in a surface are arcs. The bite of a clockwise arc stays round in the
// fill (flattened within 2.5 um), and a profile arc is written as a gr_arc, not a chord.
TEST(surface_and_profile_arcs_are_parsed) {
    std::string dir = tempDir("oc_arcs");
    writeFile(dir + "/features", "UNITS=MM\n$0 r100\nS P 0\nOB 5 10 I\nOS 15 10\nOC 15 20 15 15 Y\nOS 5 20\nOS 5 10\nOE\nSE\n");
    OdbFeatures f;
    CHECK(parseFeatures(dir + "/features", f));
    CHECK_EQ(f.surfaces.size(), (size_t)1);
    if (f.surfaces.empty() || f.surfaces[0].contours.empty()) return;
    auto& pts = f.surfaces[0].contours[0].points;
    CHECK(pts.size() > 20);
    double maxGap = 0, minX = 1e9;
    int arcEnds = 0;
    for (auto& p : pts) {
        if (p.arc) { arcEnds++; CHECK_NEAR(p.x, 15, 1e-12); CHECK_NEAR(p.y, 20, 1e-12); }
        if (p.interp) maxGap = std::max(maxGap, std::abs(std::hypot(p.x - 15, p.y - 15) - 5));
        if (p.interp) minX = std::min(minX, p.x);
    }
    CHECK_EQ(arcEnds, 1);
    CHECK(maxGap < 1e-9);          // every flattened point on the circle
    CHECK_NEAR(minX, 10, 0.003);   // clockwise from (15,10) to (15,20) bites in to x = 10

    writeFile(dir + "/profile", "UNITS=MM\nF 1\nS P 0\nOB 0 0 I\nOS 30 0\nOC 30 20 30 10 N\nOS 0 20\nOS 0 0\nOE\nSE\n");
    OdbDesign d = kicadDesign();
    CHECK(parseProfile(dir + "/profile", d.profile));
    CHECK_EQ(d.profile.units, std::string("MM"));
    KicadPcb pcb = transformToKicad(d);
    int edgeArcs = 0;
    for (auto& a : pcb.grArcs)
        if (a.layer == "Edge.Cuts") {
            edgeArcs++;
            CHECK_NEAR(a.xm, 40, 1e-9);   // counter-clockwise about (30,10): mid at x = 40
            CHECK_NEAR(a.ym, -10, 1e-9);
        }
    CHECK_EQ(edgeArcs, 1);
}

// Class 3: a pad's mask / paste opening keeps its own size (margin from the flash, turned into
// the pad frame); a second mask flash at the pin (a ring) stays drawn.
TEST(mask_and_paste_margins_come_from_the_openings) {
    OdbDesign d = kicadDesign();
    auto& u1 = component(d, "U1", 10, -10, 1);
    terminal(u1, "1", 10, -10, 1);
    terminal(u1, "2", 14, -10, 1);
    flash(d, "f.cu", 10, -10, "r2032", OdbSymbol::ROUND, 2.032);
    flash(d, "f.mask", 10, -10, "r2632", OdbSymbol::ROUND, 2.632);
    flash(d, "f.mask", 10, -10, "donut", OdbSymbol::ROUND, 4.0);
    d.layerFeatures["f.mask"].symbols.back().donut = true;
    d.layerFeatures["f.mask"].symbols.back().diameter = 4.0;
    d.layerFeatures["f.mask"].symbols.back().lineWidth = 0.5;
    flash(d, "f.cu", 14, -10, "rect2000x1000", OdbSymbol::RECT, 2.0, 1.0, {}, 90);
    flash(d, "f.mask", 14, -10, "rect1200x2200", OdbSymbol::RECT, 1.2, 2.2);   // same opening, unrotated
    flash(d, "f.paste", 14, -10, "rect1800x800", OdbSymbol::RECT, 1.8, 0.8, {}, 90);
    KicadPcb pcb = transformToKicad(d);
    const KicadPad* p1 = findPad(pcb, "U1", "1");
    const KicadPad* p2 = findPad(pcb, "U1", "2");
    CHECK(p1 && p2);
    if (!p1 || !p2) return;
    CHECK(p1->hasMaskMargin);
    CHECK_NEAR(p1->maskMargin, 0.3, 1e-9);
    CHECK(p2->hasMaskMargin);
    CHECK_NEAR(p2->maskMargin, 0.1, 1e-9);
    CHECK(p2->hasPasteMargin);
    CHECK_NEAR(p2->pasteMargin, -0.1, 1e-9);
    int rings = 0;
    for (auto& c : pcb.grCircles)
        if (c.layer == "F.Mask" && !c.filled && std::abs(c.cx - 10) < 1e-6) rings++;
    CHECK_EQ(rings, 1);
    CHECK(writeString(pcb).find("(solder_mask_margin 0.3)") != std::string::npos);
}

// Class 4: a zone's fill keeps the surface's holes (one keyholed polygon per island, not one
// filled polygon per contour); a ring flashed around a pin on its own layer is copper too.
TEST(zone_fill_keeps_its_holes) {
    OdbDesign d = gndPourAroundClearance();
    flash(d, "b.cu", 15, -15, "ring", OdbSymbol::ROUND, 3.0);
    auto& ring = d.layerFeatures["b.cu"].symbols.back();
    ring.donut = true; ring.diameter = 3.0; ring.lineWidth = 0.3;
    KicadPcb pcb = transformToKicad(d);
    const KicadZone* z = zoneOn(pcb, "B.Cu");
    CHECK(z);
    if (!z) return;
    CHECK_EQ(z->filledPolygons.size(), (size_t)1);   // the island, its hole spliced in
    if (!z->filledPolygons.empty()) CHECK(z->filledPolygons[0].size() >= 9);
    // even-odd: the hole centre is outside the fill, a point in the copper inside
    auto inside = [](const std::vector<std::pair<double,double>>& poly, double x, double y) {
        bool in = false;
        for (size_t i = 0, j = poly.size() - 1; i < poly.size(); j = i++)
            if (((poly[i].second > y) != (poly[j].second > y)) &&
                x < (poly[j].first - poly[i].first) * (y - poly[i].second) / (poly[j].second - poly[i].second) + poly[i].first)
                in = !in;
        return in;
    };
    if (!z->filledPolygons.empty()) {
        CHECK(!inside(z->filledPolygons[0], 5, 5));
        CHECK(inside(z->filledPolygons[0], 15, 15));
    }
    CHECK_EQ(z->outlinePoints.size(), (size_t)4);
    int rings = 0;
    for (auto& c : pcb.grCircles) if (c.layer == "B.Cu" && !c.filled && std::abs(c.cx - 15) < 1e-6) rings++;
    CHECK_EQ(rings, 1);
}

// Class 5: drill layers come from the matrix (TYPE=DRILL, any name) and .pad_usage=via makes a
// hit a via; START/END_NAME give a blind span. A via's mask flash is drawn (untented via).
TEST(vias_come_from_matrix_drill_layers) {
    OdbDesign d = kicadDesign();
    d.layers = {layer(1, "SOLDER_MASK", "F.MASK"), layer(2, "SIGNAL", "SIGNAL_1"), layer(3, "SIGNAL", "SIGNAL_2"),
                layer(4, "SIGNAL", "SIGNAL_3"), layer(5, "SIGNAL", "SIGNAL_4"), layer(6, "DRILL", "D_1_2"),
                layer(7, "DRILL", "D_1_4")};
    d.layers[5].startName = "SIGNAL_1"; d.layers[5].endName = "SIGNAL_2";
    d.layers[6].startName = "SIGNAL_1"; d.layers[6].endName = "SIGNAL_4";
    for (const char* cu : {"signal_1", "signal_2"}) flash(d, cu, 5, -5, "r250", OdbSymbol::ROUND, 0.25);
    flash(d, "d_1_2", 5, -5, "r100", OdbSymbol::ROUND, 0.1, -1, {{".pad_usage", "1"}});
    for (const char* cu : {"signal_1", "signal_2", "signal_3", "signal_4"}) flash(d, cu, 9, -5, "r600", OdbSymbol::ROUND, 0.6);
    flash(d, "d_1_4", 9, -5, "r300", OdbSymbol::ROUND, 0.3, -1, {{".pad_usage", "1"}});
    flash(d, "f.mask", 9, -5, "r700", OdbSymbol::ROUND, 0.7);
    KicadPcb pcb = transformToKicad(d);
    CHECK_EQ(pcb.vias.size(), (size_t)2);
    CHECK(freePads(pcb).empty());
    for (auto& v : pcb.vias) {
        if (std::abs(v.x - 5) < 1e-6) {
            CHECK(v.blind);
            CHECK_EQ(v.fromLayer, std::string("F.Cu"));
            CHECK_EQ(v.toLayer, std::string("In1.Cu"));
            CHECK_NEAR(v.drill, 0.1, 1e-9);
            CHECK_NEAR(v.size, 0.25, 1e-9);
        } else {
            CHECK(!v.blind);
            CHECK_EQ(v.toLayer, std::string("B.Cu"));
        }
    }
    int openings = 0;
    for (auto& c : pcb.grCircles) if (c.layer == "F.Mask" && c.filled && std::abs(c.cx - 9) < 1e-6) openings++;
    CHECK_EQ(openings, 1);
    CHECK(writeString(pcb).find("(via blind (at 5 5)") != std::string::npos);
}

// vias and plated through-hole pads take each copper layer's own flash (KiCad 9
// padstacks). A negative plane's flash is its clearance, not copper: the pad there is the drill's
// size and it no longer sizes the via. Such a board is written as KiCad 9 (version 20241229,
// KiCad 9 layer ids); --kicad 8 gives one size (the largest copper flash); a board with no
// per-layer difference stays KiCad 8.
static OdbDesign fourLayerPlaneDesign() {
    OdbDesign d = kicadDesign();
    d.layers = {layer(1, "COMPONENT", "COMP_+_TOP"), layer(2, "SOLDER_MASK", "F.MASK"), layer(3, "SIGNAL", "SIGT"),
                layer(4, "SIGNAL", "SIG2"), layer(5, "POWER_GROUND", "PG3"), layer(6, "SIGNAL", "SIGB"),
                layer(7, "SOLDER_MASK", "B.MASK"), layer(8, "DRILL", "DRILL")};
    d.layers[4].polarity = "NEGATIVE";
    d.layers[7].startName = "SIGT"; d.layers[7].endName = "SIGB";
    return d;
}

TEST(via_and_th_pad_sizes_are_per_layer) {
    OdbDesign d = fourLayerPlaneDesign();
    // via at (5,-5): 33 mil top, 23 mil inside and bottom, a 43 mil anti-pad on the plane
    flash(d, "sigt", 5, -5, "r33", OdbSymbol::ROUND, 0.8382);
    flash(d, "sig2", 5, -5, "r23", OdbSymbol::ROUND, 0.5842);
    flash(d, "pg3", 5, -5, "r43", OdbSymbol::ROUND, 1.0922);
    flash(d, "sigb", 5, -5, "r23", OdbSymbol::ROUND, 0.5842);
    flash(d, "drill", 5, -5, "r18", OdbSymbol::ROUND, 0.4572, -1, {{".drill", "2"}});
    // TH pin at (10,-10): square 1.65 outside, round 1.65 on SIG2, anti-pad on the plane
    auto& j = component(d, "J1", 10, -10, 2);
    terminal(j, "1", 10, -10, 1);
    flash(d, "sigt", 10, -10, "s65", OdbSymbol::SQUARE, 1.651);
    flash(d, "sigb", 10, -10, "s65", OdbSymbol::SQUARE, 1.651);
    flash(d, "sig2", 10, -10, "r65", OdbSymbol::ROUND, 1.651);
    flash(d, "pg3", 10, -10, "r85", OdbSymbol::ROUND, 2.159);
    flash(d, "drill", 10, -10, "r40", OdbSymbol::ROUND, 1.016, -1, {{".drill", "0"}});

    KicadPcb pcb = transformToKicad(d);
    CHECK_EQ(pcb.vias.size(), (size_t)1);
    if (pcb.vias.empty()) return;
    auto& v = pcb.vias[0];
    CHECK_NEAR(v.size, 0.8382, 1e-9);
    CHECK_EQ(v.layerSizes.size(), (size_t)4);
    if (v.layerSizes.size() == 4) {
        CHECK_EQ(v.layerSizes[1].first, std::string("In1.Cu"));
        CHECK_NEAR(v.layerSizes[1].second, 0.5842, 1e-9);
        CHECK_NEAR(v.layerSizes[2].second, 0.4572, 1e-9);  // plane: no ring of its own
        CHECK_NEAR(v.layerSizes[3].second, 0.5842, 1e-9);
    }
    const KicadPad* p = findPad(pcb, "J1", "1");
    CHECK(p);
    if (!p) return;
    CHECK_EQ(p->shape, std::string("rect"));
    CHECK_EQ(p->padstackMode, std::string("custom"));
    CHECK_EQ(p->padstack.size(), (size_t)2);
    if (p->padstack.size() == 2) {
        CHECK_EQ(p->padstack[0].layer, std::string("In1.Cu"));
        CHECK_EQ(p->padstack[0].shape, std::string("circle"));
        CHECK_NEAR(p->padstack[0].width, 1.651, 1e-9);
        CHECK_EQ(p->padstack[1].shape, std::string("circle"));
        CHECK_NEAR(p->padstack[1].width, 1.016, 1e-9);
    }

    std::string out = writeString(pcb);
    CHECK(out.find("(kicad_pcb (version 20241229)") != std::string::npos);
    CHECK(out.find("(2 \"B.Cu\" signal)") != std::string::npos);
    CHECK(out.find("(4 \"In1.Cu\" signal)") != std::string::npos);
    CHECK(out.find("(1 \"F.Mask\" user") != std::string::npos);
    CHECK(out.find("layerselection") == std::string::npos);
    CHECK(out.find("(size 0.8382) (drill 0.4572) (layers \"F.Cu\" \"B.Cu\")\n    (padstack (mode custom) "
                   "(layer \"In1.Cu\" (size 0.5842)) (layer \"In2.Cu\" (size 0.5842)) (layer \"B.Cu\" (size 0.5842)))\n"
                   "    (remove_unused_layers yes) (keep_end_layers yes) (zone_layer_connections \"In1.Cu\")\n    (net ")
          != std::string::npos);
    // no ring on the plane (In2.Cu) -> KiCad 9 does not flash the via there
    // (remove_unused_layers) and the padstack gives it the smallest real ring, so DRC reads no
    // 0 mm annular ring; the outer layers and In1.Cu, which have a ring, stay flashed
    CHECK(out.find("(padstack (mode custom)\n        (layer \"In1.Cu\" (shape circle) (size 1.651 1.651))\n"
                   "        (layer \"In2.Cu\" (shape circle) (size 1.016 1.016))\n"
                   "        (layer \"B.Cu\" (shape rect) (size 1.651 1.651)))") != std::string::npos);

    // KiCad 8 on request: one size, the largest copper flash (not the plane's anti-pad)
    pcb.fileFormat = 8;
    out = writeString(pcb);
    CHECK(out.find("(kicad_pcb (version 20240108)") != std::string::npos);
    CHECK(out.find("padstack") == std::string::npos);
    CHECK(out.find("(31 \"B.Cu\" signal)") != std::string::npos);
    CHECK(out.find("(via (at 5 5) (size 0.8382) (drill 0.4572)") != std::string::npos);
}

TEST(equal_inner_layers_make_a_front_inner_back_via_and_uniform_boards_stay_kicad8) {
    OdbDesign d = fourLayerPlaneDesign();
    d.layers[4].polarity = "POSITIVE";  // a positive plane: its flash is copper
    for (const char* cu : {"sigt", "sigb"}) flash(d, cu, 5, -5, "r33", OdbSymbol::ROUND, 0.8382);
    for (const char* cu : {"sig2", "pg3"}) flash(d, cu, 5, -5, "r23", OdbSymbol::ROUND, 0.5842);
    flash(d, "drill", 5, -5, "r18", OdbSymbol::ROUND, 0.4572, -1, {{".drill", "2"}});
    std::string out = writeString(transformToKicad(d));
    CHECK(out.find("(padstack (mode front_inner_back) (layer \"Inner\" (size 0.5842)) (layer \"B.Cu\" (size 0.8382)))")
          != std::string::npos);
    CHECK(out.find("remove_unused_layers") == std::string::npos);  // a ring on every layer

    OdbDesign u = fourLayerPlaneDesign();
    u.layers[4].polarity = "POSITIVE";
    for (const char* cu : {"sigt", "sig2", "pg3", "sigb"}) flash(u, cu, 5, -5, "r23", OdbSymbol::ROUND, 0.5842);
    flash(u, "drill", 5, -5, "r18", OdbSymbol::ROUND, 0.4572, -1, {{".drill", "2"}});
    KicadPcb pcb = transformToKicad(u);
    out = writeString(pcb);
    CHECK(out.find("(kicad_pcb (version 20240108)") != std::string::npos);
    CHECK(out.find("padstack") == std::string::npos);
    pcb.fileFormat = 9;  // asked for: KiCad 9 with no padstack
    out = writeString(pcb);
    CHECK(out.find("(kicad_pcb (version 20241229)") != std::string::npos);
    CHECK(out.find("padstack") == std::string::npos);
}

// Class 6: a mirrored flash (orient 9) is read clockwise, as the spec says, unless misc/info
// names KiCad, whose exporter writes that angle counter-clockwise.
TEST(mirrored_flash_angle_follows_the_producer) {
    for (const char* src : {"KiCad EDA", "Xpedition xPCB Layout", ""}) {
        OdbDesign d = kicadDesign();
        d.info.odbSource = src;
        flash(d, "b.cu", 10, -10, "rect2000x1000", OdbSymbol::RECT, 2.0, 1.0, {}, 30.0, 9);
        applyProducerQuirks(d);
        KicadPcb pcb = transformToKicad(d);
        auto fps = freePads(pcb);
        CHECK_EQ(fps.size(), (size_t)1);
        if (fps.empty()) continue;
        CHECK_NEAR(fps[0]->pads[0].angle, std::string(src) == "KiCad EDA" ? 30.0 : 330.0, 1e-9);
    }
}

// Class 7: a contour starts at OB. When the first OS does not repeat OB (Cadence), the edge out
// of OB is still drawn; when it does (KiCad), it is not drawn twice.
TEST(contours_start_at_their_first_vertex) {
    for (bool repeat : {false, true}) {
        OdbDesign d = kicadDesign();
        OdbSurface s;
        s.polarity = "P";
        OdbContour c;
        c.points = {{0, 0}, {30, 0}, {30, 10}, {10, 10}, {10, 25}, {0, 25}, {0, 0}};
        if (repeat) c.points.insert(c.points.begin() + 1, OdbPoint{0, 0});
        s.contours = {c};
        d.profile.surfaces = {s};
        KicadPcb pcb = transformToKicad(d);
        int edges = 0, firstEdge = 0;
        for (auto& l : pcb.grLines) {
            if (l.layer != "Edge.Cuts") continue;
            edges++;
            if (std::abs(l.y1) < 1e-9 && std::abs(l.y2) < 1e-9 && std::abs(std::abs(l.x2 - l.x1) - 30) < 1e-9) firstEdge++;
        }
        CHECK_EQ(edges, 6);
        CHECK_EQ(firstEdge, 1);
    }
}

// Class 8: every file is scaled by its own UNITS line (else misc/info's, else INCH), and a
// symbol's I / M suffix overrides its file's units.
TEST(units_are_per_file) {
    std::string dir = tempDir("per_file_units");
    writeFile(dir + "/misc/info", "UNITS=MM\nODB_SOURCE=made-up\n");
    writeFile(dir + "/matrix/matrix", "STEP {\n COL=1\n NAME=PCB\n}\nLAYER {\n ROW=1\n CONTEXT=BOARD\n TYPE=SIGNAL\n NAME=TOP\n POLARITY=POSITIVE\n}\n"
              "LAYER {\n ROW=2\n CONTEXT=BOARD\n TYPE=SIGNAL\n NAME=BOT\n POLARITY=POSITIVE\n}\n");
    writeFile(dir + "/steps/pcb/profile", "UNITS=MM\nS P 0\nOB 0 0 I\nOS 0 25\nOS 35 25\nOS 35 0\nOS 0 0\nOE\nSE\n");
    writeFile(dir + "/steps/pcb/layers/top/features", "UNITS=MM\n$0 r1000\n$1 rect50x30 I\nP 10 5 0 P 0 0\nP 20 5 1 P 0 0\n");
    writeFile(dir + "/steps/pcb/layers/bot/features", "UNITS=INCH\n$0 r40\nP 1 1 0 P 0 0\n");
    OdbDesign d;
    CHECK(parseOdbDirectory(dir, d));
    KicadPcb pcb = transformToKicad(d);
    auto fps = freePads(pcb);
    CHECK_EQ(fps.size(), (size_t)3);
    int found = 0;
    for (auto* fp : fps) {
        auto& p = fp->pads[0];
        if (std::abs(fp->x - 10) < 1e-6) { found++; CHECK_NEAR(p.width, 1.0, 1e-9); }
        if (std::abs(fp->x - 20) < 1e-6) { found++; CHECK_NEAR(p.width, 1.27, 1e-9); CHECK_NEAR(p.height, 0.762, 1e-9); }
        if (std::abs(fp->x - 25.4) < 1e-6) { found++; CHECK_NEAR(fp->y, -25.4, 1e-9); CHECK_NEAR(p.width, 1.016, 1e-9); }
    }
    CHECK_EQ(found, 3);
    CHECK_EQ(pcb.grRects.size(), (size_t)1);
    if (!pcb.grRects.empty()) CHECK_NEAR(std::abs(pcb.grRects[0].x2 - pcb.grRects[0].x1), 35.0, 1e-9);
}

// ---------------------------------------------------------------- round-up bug classes, part 2

static double islandsArea(const std::vector<poly::Island>& isl) {
    double a = 0;
    for (auto& i : isl) {
        a += poly::signedArea(i.outer);
        for (auto& h : i.holes) a += poly::signedArea(h);
    }
    return a;
}

static bool inPoly(const std::vector<std::pair<double,double>>& poly, double x, double y) {
    bool in = false;
    for (size_t i = 0, j = poly.size() - 1; i < poly.size(); j = i++)
        if (((poly[i].second > y) != (poly[j].second > y)) &&
            x < (poly[j].first - poly[i].first) * (y - poly[i].second) / (poly[j].second - poly[i].second) + poly[i].first)
            in = !in;
    return in;
}
static bool inFill(const KicadZone& z, double x, double y) {
    for (auto& p : z.filledPolygons) if (inPoly(p, x, y)) return true;
    return false;
}

// The difference: a hole inside, a bite out of the edge, overlapping and edge-sharing clearances,
// a clearance that splits the island in two, and a shape with a hole of its own.
TEST(polygon_difference) {
    poly::Shape a{{{0, 0}, {10, 0}, {10, 10}, {0, 10}}};
    auto sq = [](double x0, double y0, double x1, double y1) {
        return poly::Shape{{{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}}};
    };
    CHECK_NEAR(islandsArea(poly::difference({a}, {sq(2, 2, 4, 4)})), 96, 1e-6);
    auto r = poly::difference({a}, {sq(2, 2, 4, 4)});
    CHECK_EQ(r.size(), (size_t)1);
    if (!r.empty()) CHECK_EQ(r[0].holes.size(), (size_t)1);
    CHECK_NEAR(islandsArea(poly::difference({a}, {sq(-1, 4, 2, 6)})), 96, 1e-6);           // bite
    CHECK_NEAR(islandsArea(poly::difference({a}, {sq(2, 2, 5, 5), sq(4, 4, 6, 6)})), 100 - 9 - 4 + 1, 1e-6);
    CHECK_NEAR(islandsArea(poly::difference({a}, {sq(2, 2, 4, 4), sq(4, 2, 6, 4)})), 92, 1e-6);  // shared edge
    CHECK_NEAR(islandsArea(poly::difference({a}, {sq(0, 0, 10, 10)})), 0, 1e-6);            // all of it
    auto split = poly::difference({a}, {sq(4, -1, 6, 11)});
    CHECK_EQ(split.size(), (size_t)2);
    CHECK_NEAR(islandsArea(split), 80, 1e-6);
    poly::Shape donut = sq(1, 1, 9, 9);
    donut.push_back({{3, 3}, {7, 3}, {7, 7}, {3, 7}});                                     // its hole stays copper
    CHECK_NEAR(islandsArea(poly::difference({a}, {donut})), 100 - 64 + 16, 1e-6);
    CHECK_NEAR(islandsArea(poly::difference({a}, {{poly::circle(5, 5, 2)}})), 100 - 3.14159265 * 4, 0.05);
    CHECK_NEAR(islandsArea(poly::difference({a}, {{poly::stroke(-1, 5, 11, 5, 2, true)}})), 80, 1e-6);
}

// A negative flash or line clears the pour drawn before it, never one drawn after it, and is not
// itself copper (it used to be written as a segment, and a negative surface as a zone).
TEST(negative_features_clear_the_pour_before_them) {
    OdbDesign d = gndPourAroundClearance();
    auto& bcu = d.layerFeatures["b.cu"];
    int pourIdx = bcu.surfaces[0].featIdx;
    int sym = symbol(bcu, "r2000", OdbSymbol::ROUND, 2.0);
    OdbPad neg;
    neg.x = 10; neg.y = -10; neg.symIdx = sym; neg.polarity = "N"; neg.mirror = 0;
    neg.featIdx = pourIdx + 1;
    bcu.pads.push_back(neg);
    OdbPad early = neg;  // before the pour: clears nothing
    early.x = 17; early.y = -3; early.featIdx = -1;
    bcu.pads.push_back(early);
    OdbLine cut;
    cut.x1 = 0; cut.y1 = -18; cut.x2 = 20; cut.y2 = -18; cut.symIdx = sym; cut.polarity = "N";
    cut.featIdx = pourIdx + 2;
    bcu.lines.push_back(cut);
    OdbSurface negSrf;
    negSrf.polarity = "N";
    negSrf.featIdx = pourIdx + 3;
    negSrf.contours = {square(1, -10, 3, -12, false)};
    bcu.surfaces.push_back(negSrf);
    KicadPcb pcb = transformToKicad(d);
    const KicadZone* z = zoneOn(pcb, "B.Cu");
    CHECK(z);
    if (!z) return;
    CHECK(!inFill(*z, 10, 10));     // the negative flash
    CHECK(inFill(*z, 10, 12));
    CHECK(inFill(*z, 17, 3));       // the flash before the pour
    CHECK(!inFill(*z, 5, 18));      // the negative line
    CHECK(!inFill(*z, 2, 11));      // the negative surface
    CHECK(!inFill(*z, 5, 5));       // the pour's own hole
    CHECK(inFill(*z, 15, 15));
    int zones = 0;
    for (auto& zz : pcb.zones) if (zz.layer == "B.Cu") zones++;
    CHECK_EQ(zones, 1);
    for (auto& s : pcb.segments) CHECK(s.layer != "B.Cu");
}

// A NEGATIVE matrix layer is a plane: the profile less its (positive) features, on the net of the
// through-hole pins standing in its copper.
TEST(negative_layer_is_a_plane) {
    OdbDesign d = gndPourAroundClearance();
    for (auto& l : d.layers) if (l.name == "B.CU") l.polarity = "NEGATIVE";
    auto& bcu = d.layerFeatures["b.cu"];
    bcu.surfaces.clear();
    bcu.pads.erase(bcu.pads.begin() + 1);  // keep the 5,-5 antipad; 15,-15 (GND) is in the copper
    OdbSurface prof;
    prof.polarity = "P";
    prof.contours = {square(0, 0, 20, -20, false)};
    d.profile.surfaces = {prof};
    KicadPcb pcb = transformToKicad(d);
    const KicadZone* z = zoneOn(pcb, "B.Cu");
    CHECK(z);
    if (!z) return;
    CHECK(!inFill(*z, 5, 5));
    CHECK(inFill(*z, 15, 15));
    CHECK(inFill(*z, 1, 1));
    CHECK_EQ(z->netName, std::string("GND"));
    for (auto& fp : pcb.footprints) CHECK(fp.refdes.rfind("FID", 0) != 0 && fp.refdes.rfind("PAD", 0) != 0);
}

// kicad-cli writes User.1..User.9 as DOCUMENT layers USER.1..USER.9; they map back to KiCad's
TEST(user_layers_map_to_kicad_user_layers) {
    OdbDesign d = kicadDesign();
    d.layers.push_back(layer(40, "DOCUMENT", "USER.4"));
    d.layers.back().context = "MISC";
    auto& f = d.layerFeatures["user.4"];
    f.units = "MM";
    int si = symbol(f, "r200", OdbSymbol::ROUND, 0.2);
    OdbLine l;
    l.x1 = 0; l.y1 = 0; l.x2 = 5; l.y2 = 0; l.symIdx = si; l.polarity = "P";
    f.lines.push_back(l);
    KicadPcb pcb = transformToKicad(d);
    bool layerOk = false;
    for (auto& kl : pcb.layers) if (kl.canonicalName == "User.4") layerOk = kl.id == 53 && kl.type == "user";
    CHECK(layerOk);
    int lines = 0;
    for (auto& g : pcb.grLines) if (g.layer == "User.4") lines++;
    CHECK_EQ(lines, 1);
}

// A decoded symbol's islands: its outline and added shapes, less its cut shapes
static std::vector<poly::Island> symbolIslands(const OdbSymbol& sym, const poly::Shape& alsoCut = {}) {
    auto shape = [](const std::vector<std::vector<std::pair<double,double>>>& rings) {
        poly::Shape sh;
        for (auto& r : rings) sh.push_back(r);
        return sh;
    };
    std::vector<poly::Shape> pos, neg;
    if (!sym.outline.empty()) pos.push_back(shape(sym.outline));
    for (auto& a : sym.outlineAdd) pos.push_back(shape(a));
    for (auto& c : sym.outlineCut) neg.push_back(shape(c));
    if (!alsoCut.empty()) neg.push_back(alsoCut);
    return poly::difference(pos, neg);
}

static double outlineArea(const OdbSymbol& sym) { return islandsArea(symbolIslands(sym)); }

// The symbol's area inside the box (x0, y0) - (x1, y1)
static double areaIn(const OdbSymbol& sym, double x0, double y0, double x1, double y1) {
    return outlineArea(sym) - islandsArea(symbolIslands(sym, {{{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}}}));
}

// Standard symbols KiCad has no pad shape for decode to an outline (sizes / 1000, like the others)
TEST(standard_symbols_decode_to_outlines) {
    auto oct = decodeSymbol("oct1000x1000x300");
    CHECK(oct.shape == OdbSymbol::UNKNOWN);
    CHECK_NEAR(outlineArea(oct), 1.0 - 4 * 0.3 * 0.3 / 2, 1e-9);
    CHECK_NEAR(oct.width, 1.0, 1e-9);
    CHECK_NEAR(outlineArea(decodeSymbol("rect2000x1000xc300x13")), 2.0 - 2 * 0.045, 1e-9);
    CHECK_NEAR(outlineArea(decodeSymbol("rect2000x1000xc300")), 2.0 - 4 * 0.045, 1e-9);
    CHECK_NEAR(outlineArea(decodeSymbol("rect2000x1000xr300x24")), 2.0 - 2 * 0.09 * (1 - 3.14159265 / 4), 1e-3);
    CHECK_NEAR(outlineArea(decodeSymbol("el2000x1000")), 3.14159265 * 0.5, 0.01);
    CHECK_NEAR(outlineArea(decodeSymbol("di2000x1000")), 1.0, 1e-9);
    CHECK_NEAR(outlineArea(decodeSymbol("tri2000x1000")), 1.0, 1e-9);
    CHECK_NEAR(outlineArea(decodeSymbol("hex_l2000x1000x500")), 2.0 - 4 * 0.125, 1e-9);
    CHECK_NEAR(outlineArea(decodeSymbol("donut_s2000x1000")), 3.0, 1e-9);
    CHECK_NEAR(outlineArea(decodeSymbol("bfs1000")), 0.5, 1e-9);
    // a round thermal, squared ends: the ring less four 0.3 mm air gaps, in four separate pieces
    auto ths = symbolIslands(decodeSymbol("ths2000x1400x45x4x300"));
    CHECK_EQ(ths.size(), (size_t)4);
    double ring = 3.14159265 * (1.0 - 0.49);
    CHECK(islandsArea(ths) < ring - 4 * 0.3 * 0.3 * 0.9);
    CHECK(islandsArea(ths) > ring - 4 * 0.3 * 0.3 * 1.2);
    CHECK_EQ(decodeSymbol("r1000").outline.size(), (size_t)0);  // KiCad shapes keep their own path
}

// The standard symbols 0.3.0 drew wrong or not at all (ODB++ 8.1, Appendix A), areas worked by hand
TEST(standard_symbols_spec_shapes) {
    const double pi = 3.14159265358979;
    // thr: rounded copper ends. Centre line r 0.85, band 0.3; each piece stops where its end
    // circle (r 0.15) clears the 0.3 gap: asin(0.3 / 0.85) short of each spoke, plus the two caps
    auto thr = symbolIslands(decodeSymbol("thr2000x1400x45x4x300"));
    CHECK_EQ(thr.size(), (size_t)4);
    double d = std::asin(0.3 / 0.85);
    CHECK_NEAR(islandsArea(thr), 4 * ((pi / 2 - 2 * d) * 0.85 * 0.3 + pi * 0.15 * 0.15), 0.004);
    // oval_h: square left end, half circle right end
    CHECK_NEAR(outlineArea(decodeSymbol("oval_h2000x1000")), 1.5 + pi * 0.25 / 2, 2e-3);
    // s_tho: open corners; each side stops `gap` short of the next side's inside edge
    auto stho = decodeSymbol("s_tho2000x1400x45x4x300");
    CHECK_EQ(symbolIslands(stho).size(), (size_t)4);
    CHECK_NEAR(outlineArea(stho), 4 * 0.8 * 0.3, 1e-9);
    auto rctho = decodeSymbol("rc_tho3000x2000x45x4x300x300");
    CHECK_EQ(symbolIslands(rctho).size(), (size_t)4);
    CHECK_NEAR(outlineArea(rctho), 2 * 1.8 * 0.3 + 2 * 0.8 * 0.3, 1e-9);
    // rc_ths at 45 deg: the gap runs through the rectangle's corner, not along the 45 deg ray
    // from the centre (which would meet the long side 0.5 mm in from the corner)
    auto rcths = decodeSymbol("rc_ths3000x2000x45x4x300x300");
    CHECK_EQ(symbolIslands(rcths).size(), (size_t)4);
    CHECK(areaIn(rcths, 1.2, 0.7, 1.5, 1.0) < 0.01);
    CHECK_NEAR(areaIn(rcths, 0.6, 0.7, 0.9, 1.0), 0.09, 1e-6);  // the centre ray would cut here
    // rounded square thermal: the corners round, four pieces
    auto sthsr = decodeSymbol("s_ths2000x1400x0x4x300xr300");
    CHECK_EQ(symbolIslands(sthsr).size(), (size_t)4);
    CHECK(outlineArea(sthsr) < outlineArea(decodeSymbol("s_ths2000x1400x0x4x300")));
    CHECK_EQ(symbolIslands(decodeSymbol("rc_ths3000x2000x0x4x300x300xr500x13")).size(), (size_t)4);
    // line thermal: four lines, round ends
    CHECK_EQ(symbolIslands(decodeSymbol("s_thr2000x1400x45x4x300")).size(), (size_t)4);
    // oval / oblong thermals
    CHECK_EQ(symbolIslands(decodeSymbol("o_ths3000x2000x0x4x300x300")).size(), (size_t)4);
    CHECK_EQ(symbolIslands(decodeSymbol("oblong_ths3000x2000x45x4x300x300xr")).size(), (size_t)4);
    CHECK_EQ(symbolIslands(decodeSymbol("oblong_ths3000x2000x45x4x300x300xs")).size(), (size_t)4);
    // moire: rings [0.15, 0.25] and [0.4, 0.5], two 1.0 x 0.05 lines crossing them
    double moire = pi * (0.25 * 0.25 - 0.15 * 0.15) + pi * (0.25 - 0.16) + 2 * 0.05 - 0.0025 - 8 * 0.05 * 0.1;
    CHECK_NEAR(outlineArea(decodeSymbol("moire100x200x2x50x1000x0")), moire, 0.006);
    // stencil symbols
    CHECK_NEAR(outlineArea(decodeSymbol("hplate2000x1000x500")), 1.75, 1e-9);
    CHECK_NEAR(outlineArea(decodeSymbol("rhplate2000x1000x500")), 1.75, 1e-9);
    CHECK_NEAR(outlineArea(decodeSymbol("fhplate2000x1000x200x500")), 1.9, 1e-9);
    double hr = outlineArea(decodeSymbol("hplate2000x1000x500xra100xro50"));
    CHECK(hr < 1.75 && hr > 1.7);
    CHECK_NEAR(outlineArea(decodeSymbol("radhplate2000x1000x400")), 2 - pi * 0.04 / 2, 1e-3);
    CHECK_NEAR(outlineArea(decodeSymbol("dshape2000x2000x1000")), 2 + pi / 2, 3e-3);
    CHECK_NEAR(outlineArea(decodeSymbol("cross3000x2000x400x600x50x50xs")), 1.2 + 1.2 - 0.24, 1e-9);
    CHECK(outlineArea(decodeSymbol("cross3000x2000x400x600x50x50xsxra100")) > 2.16 + 4 * 0.01 * (1 - pi / 4) - 1e-4);
    CHECK_NEAR(outlineArea(decodeSymbol("dogbone3000x2000x400x600x50xs")), 2.4 + 0.72, 1e-9);
    CHECK_NEAR(outlineArea(decodeSymbol("dpack1000x1000x100x100x3x3")), 0.64, 1e-9);
    CHECK_EQ(symbolIslands(decodeSymbol("dpack1000x1000x100x100x3x3xra50")).size(), (size_t)9);
    // capitalised names decode like lower-case ones
    CHECK(decodeSymbol("RECT1000X500").shape == OdbSymbol::RECT);
    CHECK_NEAR(outlineArea(decodeSymbol("OVAL_H2000X1000")), outlineArea(decodeSymbol("oval_h2000x1000")), 1e-12);
}

// A pin or free flash of such a symbol becomes a KiCad custom pad; a graphic one a polygon
TEST(custom_symbols_become_custom_pads) {
    OdbDesign d = kicadDesign();
    auto& r1 = component(d, "R1", 10, -10, 1);
    terminal(r1, "1", 10, -10, 1);
    auto& fcu = d.layerFeatures["f.cu"];
    fcu.units = "MM";
    OdbSymbol oct = decodeSymbol("oct1000x1000x300");
    fcu.symbols.push_back(oct);
    OdbPad p;
    p.x = 10; p.y = -10; p.symIdx = (int)fcu.symbols.size() - 1; p.polarity = "P"; p.mirror = 0;
    fcu.pads.push_back(p);
    fcu.symbols.push_back(decodeSymbol("thr2000x1400x45x4x300"));
    p.x = 20; p.y = -10; p.symIdx = (int)fcu.symbols.size() - 1;
    fcu.pads.push_back(p);
    KicadPcb pcb = transformToKicad(d);
    int custom = 0;
    for (auto& fp : pcb.footprints)
        for (auto& pad : fp.pads)
            if (pad.shape == "custom") {
                custom++;
                CHECK(!pad.primitives.empty());
                if (fp.refdes == "R1") CHECK_EQ(pad.primitives.size(), (size_t)1);
                else CHECK_EQ(pad.primitives.size(), (size_t)4);
            }
    CHECK_EQ(custom, 2);
}

// symbols/<name>/features is read (in its own units) and drawn where a layer flashes <name>
TEST(user_symbols_are_drawn) {
    std::string dir = tempDir("user_symbol");
    writeFile(dir + "/misc/info", "UNITS=MM\nODB_SOURCE=made-up\n");
    writeFile(dir + "/matrix/matrix", "STEP {\n COL=1\n NAME=PCB\n}\nLAYER {\n ROW=1\n CONTEXT=BOARD\n TYPE=SIGNAL\n NAME=TOP\n POLARITY=POSITIVE\n}\n"
              "LAYER {\n ROW=2\n CONTEXT=BOARD\n TYPE=SILK_SCREEN\n NAME=SST\n POLARITY=POSITIVE\n}\n");
    writeFile(dir + "/steps/pcb/profile", "UNITS=MM\nS P 0\nOB 0 0 I\nOS 0 25\nOS 35 25\nOS 35 0\nOS 0 0\nOE\nSE\n");
    writeFile(dir + "/symbols/logo/features", "UNITS=INCH\n$0 r40\nP 0 0 0 P 0 0\nP 0.1 0 0 P 0 0\n");
    writeFile(dir + "/steps/pcb/layers/top/features", "UNITS=MM\n$0 logo\nP 10 5 0 P 0 0\n");
    writeFile(dir + "/steps/pcb/layers/sst/features", "UNITS=MM\n$0 logo\nP 20 5 0 P 0 8 90\n");
    OdbDesign d;
    CHECK(parseOdbDirectory(dir, d));
    CHECK(d.userSymbols.count("logo"));
    KicadPcb pcb = transformToKicad(d);
    auto fps = freePads(pcb);
    CHECK_EQ(fps.size(), (size_t)1);
    if (!fps.empty()) {
        CHECK_EQ(fps[0]->pads[0].shape, std::string("custom"));
        CHECK_EQ(fps[0]->pads[0].primitives.size(), (size_t)2);  // two 1.016 mm discs 2.54 mm apart
    }
    int silk = 0;
    for (auto& g : pcb.grPolys)
        if (g.layer == "F.SilkS") {
            silk++;
            double minY = 1e9, maxY = -1e9;
            for (auto& [x, y] : g.pts) { minY = std::min(minY, y); maxY = std::max(maxY, y); }
            CHECK_NEAR(maxY - minY, 1.016, 0.01);  // turned 90 deg: the discs stack along x
        }
    CHECK_EQ(silk, 2);
}

// An opening KiCad cannot make by growing the pad (a square paste window over a round pad, a
// paste window of another aspect ratio) is a paste-only pad of its own shape; a grown one stays a margin
TEST(odd_openings_become_aperture_pads) {
    OdbDesign d = kicadDesign();
    auto& u1 = component(d, "U1", 10, -10, 1);
    terminal(u1, "1", 10, -10, 1);
    terminal(u1, "2", 14, -10, 1);
    flash(d, "f.cu", 10, -10, "r300", OdbSymbol::ROUND, 0.3);
    flash(d, "f.paste", 10, -10, "rect300x300xr70", OdbSymbol::ROUNDRECT, 0.3);
    d.layerFeatures["f.paste"].symbols.back().cornerRadius = 0.07;
    flash(d, "f.mask", 10, -10, "r400", OdbSymbol::ROUND, 0.4);
    flash(d, "f.cu", 14, -10, "rect2000x1000", OdbSymbol::RECT, 2.0, 1.0);
    flash(d, "f.paste", 14, -10, "rect1600x800", OdbSymbol::RECT, 1.6, 0.8);
    KicadPcb pcb = transformToKicad(d);
    std::vector<const KicadPad*> one, two;
    for (auto& fp : pcb.footprints)
        if (fp.refdes == "U1")
            for (auto& p : fp.pads) (p.pinNum == "1" ? one : two).push_back(&p);
    CHECK_EQ(one.size(), (size_t)2);
    CHECK_EQ(two.size(), (size_t)2);
    if (one.size() == 2) {
        CHECK(hasLayer(*one[0], "F.Cu") && hasLayer(*one[0], "F.Mask") && !hasLayer(*one[0], "F.Paste"));
        CHECK(one[0]->hasMaskMargin);
        CHECK_EQ(one[1]->shape, std::string("roundrect"));
        CHECK(one[1]->layerNames == std::vector<std::string>{"F.Paste"});
        CHECK_NEAR(one[1]->width, 0.3, 1e-9);
    }
    if (two.size() == 2) {  // 0.2 / 0.1 shrink: not one margin
        CHECK(!hasLayer(*two[0], "F.Paste"));
        CHECK_EQ(two[1]->shape, std::string("rect"));
        CHECK_NEAR(two[1]->height, 0.8, 1e-9);
    }
}

// Overlapping surfaces of a layer become zones of rising priority in file order (ODB++ paints a
// later surface on top; KiCad's DRC refuses overlapping zones of one priority)
TEST(overlapping_zones_get_file_order_priorities) {
    OdbDesign d = gndPourAroundClearance();
    auto& bcu = d.layerFeatures["b.cu"];
    OdbSurface over = bcu.surfaces[0];
    over.featIdx = 100;
    over.contours = {square(10, -10, 30, -30, false)};
    bcu.surfaces.push_back(over);
    OdbSurface apart = over;
    apart.featIdx = 101;
    apart.contours = {square(50, -50, 60, -60, false)};
    bcu.surfaces.push_back(apart);
    KicadPcb pcb = transformToKicad(d);
    std::vector<int> pr;
    for (auto& z : pcb.zones) if (z.layer == "B.Cu") pr.push_back(z.priority);
    CHECK_EQ(pr.size(), (size_t)3);
    if (pr.size() == 3) {
        CHECK_EQ(pr[0], 0);
        CHECK_EQ(pr[1], 1);
        CHECK_EQ(pr[2], 0);  // overlaps nothing
    }
    CHECK(writeString(pcb).find("(priority 1)") != std::string::npos);
}

// A custom pad whose copper reaches another pad of the part on another net is a net tie
TEST(joined_custom_pads_are_net_ties) {
    OdbDesign d = kicadDesign();
    auto& st = component(d, "ST1", 10, -10, 1);
    terminal(st, "1", 9.75, -10, 1);
    terminal(st, "2", 10.25, -10, 2);
    auto& fcu = d.layerFeatures["f.cu"];
    fcu.units = "MM";
    OdbSymbol join = decodeSymbol("rect600x200xr50x14");  // part-rounded: a custom outline
    fcu.symbols.push_back(join);
    for (double x : {9.75, 10.25}) {
        OdbPad p;
        p.x = x; p.y = -10; p.symIdx = (int)fcu.symbols.size() - 1; p.polarity = "P"; p.mirror = 0;
        fcu.pads.push_back(p);
    }
    KicadPcb pcb = transformToKicad(d);
    const KicadFootprint* fp = nullptr;
    for (auto& f : pcb.footprints) if (f.refdes == "ST1") fp = &f;
    CHECK(fp);
    if (!fp) return;
    CHECK_EQ(fp->netTiePadGroups.size(), (size_t)1);
    CHECK(writeString(pcb).find("(net_tie_pad_groups \"1, 2\")") != std::string::npos);
}

// ---------------------------------------------------------------- gap closure (spec section 4)

// Twice the signed area of a ring
static double signedArea2(const std::vector<std::pair<double,double>>& r) {
    double a = 0;
    for (size_t i = 0; i < r.size(); i++) {
        auto& p = r[i];
        auto& q = r[(i + 1) % r.size()];
        a += p.first * q.second - q.first * p.second;
    }
    return a;
}

// A one-layer MM job on disk; `stephdr` is the step header's text, `extra` adds files
static std::string tinyJob(const std::string& name, const std::string& stephdr,
                           const std::vector<std::pair<std::string, std::string>>& extra = {}) {
    std::string dir = tempDir(name);
    writeFile(dir + "/misc/info", "UNITS=MM\nODB_SOURCE=made-up\n");
    writeFile(dir + "/matrix/matrix", "STEP {\n COL=1\n NAME=PCB\n}\nLAYER {\n ROW=1\n CONTEXT=BOARD\n TYPE=SIGNAL\n NAME=TOP\n POLARITY=POSITIVE\n}\n"
              "LAYER {\n ROW=2\n CONTEXT=BOARD\n TYPE=SIGNAL\n NAME=BOT\n POLARITY=POSITIVE\n}\n");
    writeFile(dir + "/steps/pcb/stephdr", stephdr);
    writeFile(dir + "/steps/pcb/profile", "UNITS=MM\nS P 0\nOB 0 0 I\nOS 0 25\nOS 35 25\nOC 35 0 35 12.5 Y\nOS 0 0\nOE\nSE\n");
    writeFile(dir + "/steps/pcb/layers/top/features",
              "UNITS=MM\n$0 r1000\n$1 r200\nP 10 5 0 P 0 0\nL 10 5 20 5 1 P 0\nA 20 5 22 7 22 5 1 P 0 Y\n"
              "S P 0\nOB 5 10 I\nOS 15 10\nOS 15 20\nOS 5 10\nOE\nSE\n");
    writeFile(dir + "/steps/pcb/layers/bot/features", "UNITS=MM\n$0 r1000\nP 10 5 0 P 0 0\n");
    for (auto& [path, text] : extra) writeFile(dir + "/" + path, text);
    return dir;
}

// 4.1: every coordinate of a step is relative to the stephdr origin (x' = x + X_ORIGIN,
// y' = y + Y_ORIGIN), in the stephdr's own units; the datum is not a transform.
TEST(stephdr_origin_moves_the_step) {
    const std::string comps = "UNITS=MM\nCMP 0 10 5 0 N R1 pkg\nTOP 0 10 5 0 N 0 0 1\n";
    const std::string netlist = "UNITS=MM\n$0 N1\n#\n#Netlist points\n#\n0 0.5 10 5 T 1 1 e e s\n";
    std::vector<std::pair<std::string, std::string>> extra = {
        {"steps/pcb/layers/comp_+_top/components", comps}, {"steps/pcb/netlists/cadnet/netlist", netlist}};
    OdbDesign plain, moved;
    CHECK(parseOdbDirectory(tinyJob("origin_plain", "UNITS=MM\nX_DATUM=3\nY_DATUM=4\n", extra), plain));
    // 0.1 in = 2.54 mm right, 0.2 in = 5.08 mm down, written in inches
    CHECK(parseOdbDirectory(tinyJob("origin_moved", "UNITS=INCH\nX_ORIGIN=0.1\nY_ORIGIN=-0.2\nX_DATUM=3\nY_DATUM=4\n", extra), moved));
    const double ox = 2.54, oy = -5.08;
    CHECK(!plain.stepHeader.originApplied);
    CHECK(moved.stepHeader.originApplied);
    CHECK_NEAR(moved.stepHeader.appliedX, ox, 1e-9);
    CHECK_NEAR(moved.stepHeader.appliedY, oy, 1e-9);
    auto& a = plain.layerFeatures.at("top");
    auto& b = moved.layerFeatures.at("top");
    CHECK_NEAR(b.pads[0].x, a.pads[0].x + ox, 1e-9);
    CHECK_NEAR(b.pads[0].y, a.pads[0].y + oy, 1e-9);
    CHECK_NEAR(b.lines[0].x2, a.lines[0].x2 + ox, 1e-9);
    CHECK_NEAR(b.lines[0].y2, a.lines[0].y2 + oy, 1e-9);
    CHECK_NEAR(b.arcs[0].xc, a.arcs[0].xc + ox, 1e-9);
    CHECK_NEAR(b.arcs[0].ye, a.arcs[0].ye + oy, 1e-9);
    CHECK_NEAR(b.surfaces[0].contours[0].points[1].x, a.surfaces[0].contours[0].points[1].x + ox, 1e-9);
    CHECK_NEAR(b.surfaces[0].contours[0].points[1].y, a.surfaces[0].contours[0].points[1].y + oy, 1e-9);
    // the profile, with the centre of its OC arc
    bool sawArc = false;
    auto& pa = plain.profile.surfaces[0].contours[0].points;
    auto& pb = moved.profile.surfaces[0].contours[0].points;
    CHECK_EQ(pa.size(), pb.size());
    for (size_t i = 0; i < pa.size() && i < pb.size(); i++) {
        CHECK_NEAR(pb[i].x, pa[i].x + ox, 1e-9);
        CHECK_NEAR(pb[i].y, pa[i].y + oy, 1e-9);
        if (pa[i].arc) { sawArc = true; CHECK_NEAR(pb[i].xc, pa[i].xc + ox, 1e-9); CHECK_NEAR(pb[i].yc, pa[i].yc + oy, 1e-9); }
    }
    CHECK(sawArc);
    CHECK_NEAR(moved.topComponents.components[0].x, 10 + ox, 1e-9);
    CHECK_NEAR(moved.topComponents.components[0].terminals[0].y, 5 + oy, 1e-9);
    CHECK_NEAR(moved.netlist.points[0].x, 10 + ox, 1e-9);
    // the written board: KiCad (x, y) = (x + ox, -(y + oy)); the pad stays on its footprint
    KicadPcb p0 = transformToKicad(plain), p1 = transformToKicad(moved);
    auto* f0 = findFp(p0, "R1");
    auto* f1 = findFp(p1, "R1");
    CHECK(f0 && f1);
    if (f0 && f1) {
        CHECK_NEAR(f1->x, f0->x + ox, 1e-9);
        CHECK_NEAR(f1->y, f0->y - oy, 1e-9);
        CHECK_NEAR(f1->pads[0].x, f0->pads[0].x, 1e-9);
    }
    // a second call moves nothing: the origin is consumed
    double before = moved.layerFeatures.at("top").pads[0].x;
    applyStepOrigin(moved);
    CHECK_NEAR(moved.layerFeatures.at("top").pads[0].x, before, 1e-12);
}

// 4.2: a netlist point is <net> <radius> <x> <y> <side> [<w> <h>] <epoint> <exp> ... [v]. The
// letters after <side> used to be read as numbers, which lost the rest of the line and with it
// the trailing "v" that marks a via point.
TEST(netlist_points_are_read_as_tokens) {
    std::string dir = tempDir("netlist_tokens");
    writeFile(dir + "/netlist",
              "H optimize n staggered n\n$0 GND\n$1 +3V3\n#\n#Netlist points\n#\n"
              "0 0.30 120.0 -102.0 B e c staggered 0 0 0 v\n"   // odb-kitchen-sink: a covered via
              "0 0.30 118.0 -102.0 B e e staggered 0 0 0 v\n"
              "1 0 124.38 -90.90 T 0.80 0.95 e s\n"             // an SMD pad: w and h, no hole
              "1 1.0 55.0 -20.0 B e e staggered 0 0 0\n"        // odb-rotation: a pin hole, not a via
              "1 0.009 0.6 0.475 B e e staggered 0 0 0 v \n"    // demo-design: trailing blank
              "0 0.002 4.825 3.965 T e e \n"
              "-1 0 1 2 T 0.5 0.5 m e\n"
              "0 x 1 2 T e e v\n");                             // not a point: skipped
    OdbNetlist nl;
    CHECK(parseNetlist(dir + "/netlist", nl));
    CHECK_EQ(nl.nets.size(), (size_t)2);
    CHECK_EQ(nl.points.size(), (size_t)7);
    if (nl.points.size() != 7) return;
    auto& p = nl.points;
    CHECK(p[0].isVia && p[1].isVia && p[4].isVia);
    CHECK(!p[2].isVia && !p[3].isVia && !p[5].isVia && !p[6].isVia);
    CHECK_NEAR(p[0].radius, 0.30, 1e-12);
    CHECK(!p[0].hasPadSize);
    CHECK_EQ(p[0].layer, std::string("B"));
    CHECK_EQ(p[0].epoint, std::string("e"));
    CHECK_EQ(p[0].exp, std::string("c"));
    CHECK(p[2].hasPadSize);
    CHECK_NEAR(p[2].padW, 0.80, 1e-12);
    CHECK_NEAR(p[2].padH, 0.95, 1e-12);
    CHECK_EQ(p[2].exp, std::string("s"));
    CHECK_NEAR(p[2].radius, 0.0, 1e-12);
    CHECK_NEAR(p[3].radius, 1.0, 1e-12);
    CHECK_EQ(p[3].netIdx, 1);
    CHECK_NEAR(p[4].x, 0.6, 1e-12);
    CHECK_EQ(p[6].netIdx, -1);
    CHECK_EQ(p[6].epoint, std::string("m"));
}

// 4.2: a via with no FID record takes the net of the netlist point marked "v" at its place, not
// of whatever other point happens to sit there
TEST(via_net_comes_from_the_via_netlist_point) {
    OdbDesign d = kicadDesign();
    d.edaData.nets.clear();  // no eda/data: nets come from the netlist
    d.netlist.nets = {{0, "GND"}, {1, "SIG"}};
    flash(d, "f.cu", 5, -5, "r600", OdbSymbol::ROUND, 0.6);
    flash(d, "b.cu", 5, -5, "r600", OdbSymbol::ROUND, 0.6);
    flash(d, "drill_plated_f.cu-b.cu", 5, -5, "r300", OdbSymbol::ROUND, 0.3, -1, {{".pad_usage", "1"}});
    OdbNetPoint other, via;
    other.netIdx = 0; other.x = 5.004; other.y = -5; other.layer = "T"; other.hasPadSize = true;
    via.netIdx = 1; via.radius = 0.15; via.x = 5; via.y = -5; via.layer = "B"; via.isVia = true;
    d.netlist.points = {other, via};
    KicadPcb pcb = transformToKicad(d);
    CHECK_EQ(pcb.vias.size(), (size_t)1);
    if (pcb.vias.empty()) return;
    std::string name;
    for (auto& n : pcb.nets) if (n.id == pcb.vias[0].netId) name = n.name;
    CHECK_EQ(name, std::string("SIG"));
}

// 4.3: the tools file grammar: THICKNESS, USER_PARAMS, TOOLS { NUM TYPE [TYPE2] MIN_TOL MAX_TOL
// BIT FINISH_SIZE DRILL_SIZE }; lenient about empty values, unknown keys and an empty file
TEST(drill_tools_file_is_parsed) {
    std::string dir = tempDir("tools_grammar");
    writeFile(dir + "/tools",
              "THICKNESS=62\nUSER_PARAMS=PANEL_PLATE\n\n"
              "TOOLS {\n    NUM=1\n    TYPE=VIA\n    MIN_TOL=3\n    MAX_TOL=3\n    BIT=+77\n    FINISH_SIZE=13.5\n    DRILL_SIZE=18\n}\n\n"
              "TOOLS {\n    NUM=2\n    TYPE=non_plated\n    TYPE2=STANDARD\n    MIN_TOL=0\n    MAX_TOL=\n    BIT=\n    SOMETHING_NEW=1\n"
              "    FINISH_SIZE=-9.84e-05\n    DRILL_SIZE=125\n}\n"
              "TOOLS {\n    TYPE=PLATED\n    DRILL_SIZE=40\n}\n");  // no NUM: not a tool
    OdbDrillTools t;
    CHECK(parseDrillTools(dir + "/tools", t));
    CHECK(t.present);
    CHECK(t.units.empty());
    CHECK_NEAR(t.thickness, 62, 1e-12);
    CHECK_EQ(t.userParams, std::string("PANEL_PLATE"));
    CHECK_EQ(t.tools.size(), (size_t)2);
    if (t.tools.size() != 2) return;
    CHECK_EQ(t.tools[0].num, 1);
    CHECK_EQ(t.tools[0].type, std::string("VIA"));
    CHECK_EQ(t.tools[0].bit, std::string("+77"));
    CHECK_NEAR(t.tools[0].minTol, 3, 1e-12);
    CHECK_NEAR(t.tools[0].finishSize, 13.5, 1e-12);
    CHECK_NEAR(t.tools[0].drillSize, 18, 1e-12);
    CHECK_EQ(t.tools[1].type, std::string("NON_PLATED"));
    CHECK_EQ(t.tools[1].type2, std::string("STANDARD"));
    CHECK(t.tools[1].bit.empty());
    CHECK_NEAR(t.tools[1].maxTol, 0, 1e-12);
    CHECK_NEAR(t.tools[1].drillSize, 125, 1e-12);

    writeFile(dir + "/empty", "");
    OdbDrillTools e, none;
    CHECK(parseDrillTools(dir + "/empty", e));
    CHECK(e.present && e.tools.empty());
    CHECK(!parseDrillTools(dir + "/absent", none));
    CHECK(!none.present);
}

// A drill-layer job on disk: F.CU / B.CU, one DRILL row, the given drill features and tools files
static std::string drillJob(const std::string& name, const std::string& drillFeatures, const std::string& tools) {
    std::string dir = tempDir(name);
    writeFile(dir + "/matrix/matrix",
              "STEP {\n COL=1\n NAME=PCB\n}\n"
              "LAYER {\n ROW=1\n CONTEXT=BOARD\n TYPE=SIGNAL\n NAME=TOP\n POLARITY=POSITIVE\n}\n"
              "LAYER {\n ROW=2\n CONTEXT=BOARD\n TYPE=SIGNAL\n NAME=BOT\n POLARITY=POSITIVE\n}\n"
              "LAYER {\n ROW=3\n CONTEXT=BOARD\n TYPE=DRILL\n NAME=DRL\n POLARITY=POSITIVE\n}\n");
    writeFile(dir + "/steps/pcb/profile", "S P 0\nOB 0 0 I\nOS 0 2\nOS 3 2\nOS 3 0\nOS 0 0\nOE\nSE\n");
    // no UNITS line anywhere: INCH. Copper flashes at every hit.
    std::string cu = "$0 r50\n";
    for (const char* xy : {"0.5 0.5", "1.0 0.5", "1.5 0.5", "2.0 0.5", "2.5 0.5"}) cu += std::string("P ") + xy + " 0 P 0 0\n";
    writeFile(dir + "/steps/pcb/layers/top/features", cu);
    writeFile(dir + "/steps/pcb/layers/bot/features", cu);
    writeFile(dir + "/steps/pcb/layers/drl/features", drillFeatures);
    if (tools != "-") writeFile(dir + "/steps/pcb/layers/drl/tools", tools);
    return dir;
}

static double viaDrillAt(const KicadPcb& pcb, double xIn) {
    for (auto& v : pcb.vias) if (std::abs(v.x - xIn * 25.4) < 1e-6) return v.drill;
    return -1;
}

// 4.3: the drill size is the tools table's DRILL_SIZE (thousandths of the file's unit: mils for
// INCH), found by the hit's dcode (= tool NUM), its .bit string, a TOOL<n> string or its symbol's
// size; the flash symbol only when the table names no tool for the hit.
TEST(drill_sizes_come_from_the_tools_table) {
    const std::string feats =
        "$0 r17\n$1 r30\n$2 r99\n@0 .drill\n@1 .bit\n@2 .geometry\n&0 +77\n&1 TOOL3\n"
        "P 0.5 0.5 0 P 1 0;0=2\n"        // dcode 1 -> tool 1: 18 mil, though the symbol says r17
        "P 1.0 0.5 0 P 0 0;0=2,1=0\n"    // no dcode, .bit "+77" -> tool 1
        "P 1.5 0.5 1 P 0 0;0=2,2=1\n"    // .geometry "TOOL3" -> tool 3: 30.5 mil
        "P 2.0 0.5 1 P 0 0;0=2\n"        // nothing but the symbol: r30 has no tool of its size
        "P 2.5 0.5 2 P 1 0;0=2\n";       // dcode 1, but r99 is not tool 1's size: the symbol stands
    const std::string tools =
        "THICKNESS=62\nUSER_PARAMS=\n"
        "TOOLS {\n NUM=1\n TYPE=VIA\n MIN_TOL=3\n MAX_TOL=3\n BIT=+77\n FINISH_SIZE=13.5\n DRILL_SIZE=18\n}\n"
        "TOOLS {\n NUM=3\n TYPE=VIA\n MIN_TOL=0\n MAX_TOL=0\n BIT=\n FINISH_SIZE=28\n DRILL_SIZE=30.5\n}\n";
    OdbDesign d;
    CHECK(parseOdbDirectory(drillJob("tools_sizes", feats, tools), d));
    CHECK(d.layerTools.count("drl") == 1);
    CHECK_NEAR(d.layerTools["drl"].thickness, 62 * 0.0254, 1e-9);        // mils -> mm
    CHECK_NEAR(d.layerTools["drl"].tools[0].drillSize, 0.4572, 1e-9);    // DRILL_SIZE=18 -> 0.4572 mm
    CHECK_NEAR(d.layerTools["drl"].tools[0].finishSize, 13.5 * 0.0254, 1e-9);
    CHECK_EQ(d.layerFeatures["drl"].pads[0].dcode, 1);
    KicadPcb pcb = transformToKicad(d);
    CHECK_EQ(pcb.vias.size(), (size_t)5);
    CHECK_NEAR(viaDrillAt(pcb, 0.5), 0.4572, 1e-9);
    CHECK_NEAR(viaDrillAt(pcb, 1.0), 0.4572, 1e-9);
    CHECK_NEAR(viaDrillAt(pcb, 1.5), 30.5 * 0.0254, 1e-9);
    CHECK_NEAR(viaDrillAt(pcb, 2.0), 30 * 0.0254, 1e-9);
    CHECK_NEAR(viaDrillAt(pcb, 2.5), 99 * 0.0254, 1e-9);
    CHECK(writeString(pcb).find("(drill 0.4572)") != std::string::npos);

    // no tools file (or an empty one): the symbol, as before
    for (const char* t : {"-", ""}) {
        OdbDesign n;
        CHECK(parseOdbDirectory(drillJob(std::string("tools_none") + (*t ? "1" : "0"), feats, t), n));
        KicadPcb p = transformToKicad(n);
        CHECK_NEAR(viaDrillAt(p, 0.5), 17 * 0.0254, 1e-9);
        CHECK_NEAR(viaDrillAt(p, 1.5), 30 * 0.0254, 1e-9);
    }
}

// 4.3: a hit without .drill / .pad_usage takes plated / non-plated / via from its tool's TYPE; a
// size shared by tools of two types gives the size only (KiCad writes one tool per hit, dcode 0)
TEST(drill_kind_comes_from_the_tool_type) {
    const std::string feats =
        "UNITS=MM\n$0 r300\n$1 r1000\n$2 r2000\n"
        "P 12.7 12.7 0 P 1 0\n"     // tool 1 VIA
        "P 25.4 12.7 1 P 2 0\n"     // tool 2 NON_PLATED
        "P 38.1 12.7 2 P 0 0\n";    // by size: tools 3 and 4 are both 2 mm, one plated, one not
    const std::string tools =
        "UNITS=MM\nTHICKNESS=1600\n"
        "TOOLS {\n NUM=1\n TYPE=VIA\n TYPE2=STANDARD\n DRILL_SIZE=300.0\n}\n"
        "TOOLS {\n NUM=2\n TYPE=NON_PLATED\n DRILL_SIZE=1000.0\n}\n"
        "TOOLS {\n NUM=3\n TYPE=NON_PLATED\n DRILL_SIZE=2000.0\n}\n"
        "TOOLS {\n NUM=4\n TYPE=PLATED\n DRILL_SIZE=2000.0\n}\n";
    OdbDesign d;
    CHECK(parseOdbDirectory(drillJob("tools_types", feats, tools), d));
    CHECK_NEAR(d.layerTools["drl"].thickness, 1.6, 1e-9);   // microns -> mm
    KicadPcb pcb = transformToKicad(d);
    CHECK_EQ(pcb.vias.size(), (size_t)1);
    if (!pcb.vias.empty()) CHECK_NEAR(pcb.vias[0].drill, 0.3, 1e-9);
    int np = 0, plated = 0;
    for (auto& fp : pcb.footprints)
        for (auto& pad : fp.pads) {
            if (pad.type == "np_thru_hole") { np++; CHECK_NEAR(pad.drill, 1.0, 1e-9); }
            if (pad.type == "thru_hole") { plated++; CHECK_NEAR(pad.drill, 2.0, 1e-9); }
        }
    CHECK_EQ(np, 1);
    CHECK_EQ(plated, 1);
}

// 4.4: attrlist files are "key = value" lines; empty and missing files are legal, unknown keys kept
TEST(attrlist_is_lenient) {
    std::string dir = tempDir("attrlist");
    writeFile(dir + "/a", "UNITS=INCH\n.layer_dielectric=0.00255906\n.out_x_scale = 1.000000\n.out_mirror = no\n"
                          ".eda_layers = \"L1\",\"L2\"\n.comment = \nno equals sign here\n# a comment\n = orphan\nserial_number = 15\n");
    OdbAttrList a;
    CHECK(parseAttrList(dir + "/a", a));
    CHECK(a.present);
    CHECK_EQ(a.units, std::string("INCH"));
    CHECK_NEAR(a.number(".layer_dielectric"), 0.00255906, 1e-12);
    CHECK_NEAR(a.number(".out_x_scale", 7), 1.0, 1e-12);
    CHECK_NEAR(a.number(".out_y_scale", 7), 7.0, 1e-12);     // absent: the fallback
    CHECK_NEAR(a.number(".comment", 7), 7.0, 1e-12);         // empty: the fallback
    CHECK_NEAR(a.number(".eda_layers", 7), 7.0, 1e-12);      // not a number: the fallback
    CHECK(!a.flag(".out_mirror"));
    CHECK(a.has(".eda_layers") && !a.has(".comment") && !a.has("nope"));
    CHECK_EQ(a.text(".eda_layers"), std::string("\"L1\",\"L2\""));
    CHECK_EQ(a.text("serial_number"), std::string("15"));    // a key the reader has no use for is kept
    CHECK_EQ(a.values.size(), (size_t)6);

    writeFile(dir + "/empty", "");
    OdbAttrList e, none;
    CHECK(parseAttrList(dir + "/empty", e));
    CHECK(e.present && e.values.empty());
    CHECK(!parseAttrList(dir + "/absent", none));
    CHECK(!none.present);
}

// A 4-layer job with a stackup: `attrs` are (layer folder, attrlist text) pairs
static std::string stackJob(const std::string& name, const std::vector<std::pair<std::string, std::string>>& attrs,
                            const std::string& tools = "") {
    std::string dir = tempDir(name);
    writeFile(dir + "/misc/info", "UNITS=MM\n");
    std::string m = "STEP {\n COL=1\n NAME=PCB\n}\n";
    const char* rows[][2] = {{"SOLDER_MASK", "SMT"}, {"SIGNAL", "L1"}, {"DIELECTRIC", "D1"}, {"POWER_GROUND", "L2"},
                             {"DIELECTRIC", "D2"}, {"SIGNAL", "L3"}, {"DIELECTRIC", "D3"}, {"SIGNAL", "L4"},
                             {"SOLDER_MASK", "SMB"}, {"DRILL", "DRL"}};
    int row = 1;
    for (auto& r : rows)
        m += "LAYER {\n ROW=" + std::to_string(row++) + "\n CONTEXT=BOARD\n TYPE=" + r[0] + "\n NAME=" + r[1] + "\n POLARITY=POSITIVE\n}\n";
    writeFile(dir + "/matrix/matrix", m);
    writeFile(dir + "/steps/pcb/profile", "UNITS=MM\nS P 0\nOB 0 0 I\nOS 0 20\nOS 30 20\nOS 30 0\nOS 0 0\nOE\nSE\n");
    writeFile(dir + "/steps/pcb/layers/l1/features",
              "UNITS=MM\n$0 rect2000x1000\n$1 r200\nP 10 5 0 P 0 8 30\nP 4 2 0 P 0 1\nL 10 5 20 5 1 P 0\nA 20 5 22 7 22 5 1 P 0 Y\n"
              "S P 0\nOB 5 10 I\nOS 15 10\nOC 15 14 15 12 Y\nOS 5 10\nOE\nSE\n");
    writeFile(dir + "/steps/pcb/layers/l4/features", "UNITS=MM\n$0 r1000\nP 10 5 0 P 0 0\n");
    writeFile(dir + "/steps/pcb/layers/drl/features", "UNITS=MM\n$0 r300\n");
    if (!tools.empty()) writeFile(dir + "/steps/pcb/layers/drl/tools", tools);
    for (auto& [layerDir, text] : attrs) writeFile(dir + "/steps/pcb/layers/" + layerDir + "/attrlist", text);
    return dir;
}

// 4.4: board thickness is the stackup the attrlists describe (dielectric rows, plus copper and
// mask), else a drill table's THICKNESS, else the 1.6 mm default; the report says which
TEST(board_thickness_comes_from_the_job) {
    auto reportHas = [](const OdbDesign& d, const std::string& what) {
        for (auto& l : d.report) if (l.find(what) != std::string::npos) return true;
        return false;
    };
    {   // attrlists in INCH on a MM job: 3 dielectric rows, copper by weight, one mask row
        OdbDesign d;
        CHECK(parseOdbDirectory(stackJob("thick_attr", {
            {"d1", "UNITS=INCH\n.layer_dielectric=0.008\n"}, {"d2", "UNITS=INCH\n.layer_dielectric=0.04\n"},
            {"d3", "UNITS=INCH\n.layer_dielectric=0.008\n"},
            {"l1", ".copper_weight=1\n"}, {"l2", ".copper_weight=0.5\n"}, {"l3", ".copper_weight=0.5\n"},
            {"l4", ".copper_weight=1\n.layer_dielectric=9\n"},     // the weight wins over the row's own thickness
            {"smt", ".layer_dielectric=0.0152\n"}, {"smb", ""}},
            "UNITS=MM\nTHICKNESS=1234\n"), d));
        // 56 mil = 1.4224, 3 oz = 0.1044, mask 0.0152 -> 1.542, kept to the micron
        CHECK_NEAR(d.boardThickness, 1.542, 1e-9);
        CHECK_EQ(d.thicknessSource, std::string("attrlist"));
        CHECK(reportHas(d, "from the layer attrlists (3 dielectric rows"));
        CHECK_NEAR(transformToKicad(d).boardThickness, d.boardThickness, 1e-12);
        CHECK(writeString(transformToKicad(d)).find("(thickness 1.542)") != std::string::npos);
    }
    {   // KiCad's way: every row carries its own thickness in the job's units, copper a weight too
        OdbDesign d;
        CHECK(parseOdbDirectory(stackJob("thick_kicad", {
            {"d2", ".layer_dielectric=1.51\n.material=fr4\n"}, {"l1", ".layer_dielectric=0.03\n.copper_weight=1.0\n"},
            {"l4", ".layer_dielectric=0.03\n.copper_weight=1.0\n"}, {"smt", ".layer_dielectric=0.01\n"},
            {"smb", ".layer_dielectric=0.01\n"}, {"d1", ""}, {"d3", ""}}), d));
        CHECK_NEAR(d.boardThickness, 1.6, 1e-9);
    }
    {   // no dielectric data, but the drill table knows what it went through (62 mil)
        OdbDesign d;
        CHECK(parseOdbDirectory(stackJob("thick_tools", {{"l1", ".copper_weight=1\n"}, {"d1", ""}},
                                         "THICKNESS=62\nUSER_PARAMS=\n"), d));   // no UNITS: the job's MM -> microns
        CHECK_NEAR(d.boardThickness, 0.062, 1e-9);
        CHECK_EQ(d.thicknessSource, std::string("tools"));
        OdbDesign in;
        CHECK(parseOdbDirectory(stackJob("thick_tools_in", {}, "UNITS=INCH\nTHICKNESS=62\n"), in));
        CHECK_NEAR(in.boardThickness, 1.5748, 1e-9);
        CHECK(reportHas(in, "from THICKNESS of layers/drl/tools"));
    }
    {   // spec v2 3.9: an explicit .board_thickness is the authority, over the stackup rows and
        // the drill table; the step's attrlist first, then the job's (each in its own units)
        std::string dir = stackJob("thick_explicit", {{"d1", ".layer_dielectric=0.5\n"}}, "UNITS=MM\nTHICKNESS=1234\n");
        writeFile(dir + "/misc/attrlist", "UNITS=INCH\n.comment=x\n.board_thickness=0.0547402\n");
        OdbDesign d;
        CHECK(parseOdbDirectory(dir, d));
        CHECK_NEAR(d.boardThickness, 1.3904, 1e-9);
        CHECK_EQ(d.thicknessSource, std::string("board_thickness"));
        CHECK(reportHas(d, "from .board_thickness of misc/attrlist"));
        CHECK(writeString(transformToKicad(d)).find("(thickness 1.3904)") != std::string::npos);
        writeFile(dir + "/steps/pcb/attrlist", ".board_thickness = 2.4\n");
        OdbDesign e;
        CHECK(parseOdbDirectory(dir, e));
        CHECK_NEAR(e.boardThickness, 2.4, 1e-9);
        CHECK(reportHas(e, "from .board_thickness of steps/"));
        writeFile(dir + "/steps/pcb/attrlist", ".board_thickness = 0\n");
        writeFile(dir + "/misc/attrlist", ".board_thickness =\n");
        OdbDesign z;                                 // empty or zero: not a thickness
        CHECK(parseOdbDirectory(dir, z));
        CHECK_EQ(z.thicknessSource, std::string("attrlist"));
    }
    {   // nothing: the default, and the report says so
        OdbDesign d;
        CHECK(parseOdbDirectory(stackJob("thick_none", {{"d1", ""}, {"l1", ".eda_layers = \"TOP\"\n"}}), d));
        CHECK_NEAR(d.boardThickness, 0.0, 1e-12);
        CHECK_EQ(d.thicknessSource, std::string("default"));
        CHECK(reportHas(d, "1.6 mm by default"));
        CHECK_NEAR(transformToKicad(d).boardThickness, 1.6, 1e-12);
    }
}

// 4.4: a layer's .out_x_scale / .out_y_scale / .out_mirror / .out_angle move that layer's
// features (scale about zero, mirror in X, turn clockwise) and no other layer; identity values
// (what every sample carries) change nothing
TEST(layer_output_transform_is_applied) {
    OdbDesign plain, same, scaled, turned;
    CHECK(parseOdbDirectory(stackJob("out_plain", {}), plain));
    CHECK(parseOdbDirectory(stackJob("out_same", {{"l1", ".out_x_scale = 1.000000\n.out_y_scale = 1.000000\n.out_mirror = no\n.out_angle = 0.0\n.out_polarity = positive\n"}}), same));
    CHECK(parseOdbDirectory(stackJob("out_scaled", {{"l1", ".out_x_scale = 0.5\n.out_y_scale = 0.5\n"}}), scaled));
    CHECK(parseOdbDirectory(stackJob("out_turned", {{"l1", ".out_mirror = yes\n.out_angle = 90\n.out_x_scale = 2\n"}}), turned));
    auto& a = plain.layerFeatures.at("l1");
    {
        auto& b = same.layerFeatures.at("l1");
        CHECK_NEAR(b.pads[0].x, a.pads[0].x, 1e-12);
        CHECK_EQ(b.pads[0].mirror, a.pads[0].mirror);
        CHECK_EQ(b.pads[1].mirror, 1);            // a legacy orient code is left as written
        CHECK_NEAR(b.symbols[0].width, a.symbols[0].width, 1e-12);
        for (auto& l : same.report) CHECK(l.find("output transform") == std::string::npos);
    }
    {
        auto& b = scaled.layerFeatures.at("l1");
        CHECK_NEAR(b.pads[0].x, 5, 1e-9);
        CHECK_NEAR(b.pads[0].y, 2.5, 1e-9);
        CHECK_NEAR(b.pads[0].angle, 30, 1e-9);
        CHECK_NEAR(b.symbols[0].width, 1.0, 1e-9);   // an even scale takes the symbols with it
        CHECK_NEAR(b.symbols[1].diameter, 0.1, 1e-9);
        CHECK_NEAR(b.lines[0].x2, 10, 1e-9);
        CHECK_NEAR(b.arcs[0].xc, 11, 1e-9);
        CHECK(b.arcs[0].clockwise);
        CHECK_NEAR(b.surfaces[0].contours[0].points[1].x, 7.5, 1e-9);
        // the other layers and the profile stay
        CHECK_NEAR(scaled.layerFeatures.at("l4").pads[0].x, 10, 1e-12);
        CHECK_NEAR(scaled.profile.surfaces[0].contours[0].points[2].x, 30, 1e-12);
        bool said = false;
        for (auto& l : scaled.report) said = said || l.find("layer l1 drawn with its output transform") != std::string::npos;
        CHECK(said);
    }
    {   // x * 2, then x -> -x, then 90 deg clockwise: (x, y) -> (y, 2x)
        auto& b = turned.layerFeatures.at("l1");
        CHECK_NEAR(b.pads[0].x, 5, 1e-9);
        CHECK_NEAR(b.pads[0].y, 20, 1e-9);
        CHECK_EQ(b.pads[0].mirror, 9);                 // mirrored, and its 30 deg now turns the other way
        CHECK_NEAR(b.pads[0].angle, 60, 1e-9);         // -30 + 90
        CHECK_EQ(b.pads[1].mirror, 9);                 // orient 1 (90 deg): -90 + 90 = 0, mirrored
        CHECK_NEAR(b.pads[1].angle, 0, 1e-9);
        CHECK_NEAR(b.symbols[0].width, 2.0, 1e-9);     // an uneven scale leaves the symbols alone
        CHECK_NEAR(b.lines[0].x2, 5, 1e-9);
        CHECK_NEAR(b.lines[0].y2, 40, 1e-9);
        CHECK(!b.arcs[0].clockwise);                   // a mirror turns arcs the other way
        bool sawArc = false;
        for (auto& p : b.surfaces[0].contours[0].points) if (p.arc) { sawArc = true; CHECK(!p.cw); }
        CHECK(sawArc);
    }
}

// A job with steps PCB (COL 1), FLEX (COL 2, listed first), an empty PANEL (COL 3) and a folder
// "extra" the matrix does not name; component rows COMP_+_TOP, COMP_+_LEFT (above the copper)
// and COMP_+_RIGHT (below it)
static std::string twoStepJob(const std::string& name) {
    std::string dir = tempDir(name);
    writeFile(dir + "/misc/info", "JOB_NAME=demo job\nUNITS=MM\n");
    std::string m = "STEP {\n COL=2\n NAME=FLEX\n}\nSTEP {\n COL=3\n NAME=PANEL\n}\nSTEP {\n COL=1\n NAME=PCB\n}\n";
    const char* rows[][2] = {{"COMPONENT", "COMP_+_TOP"}, {"COMPONENT", "COMP_+_LEFT"}, {"SIGNAL", "TOP"}, {"SIGNAL", "BOT"},
                             {"COMPONENT", "COMP_+_RIGHT"}, {"COMPONENT", "COMP_+_BOT"}};
    int row = 1;
    for (auto& r : rows)
        m += "LAYER {\n ROW=" + std::to_string(row++) + "\n CONTEXT=BOARD\n TYPE=" + r[0] + "\n NAME=" + r[1] + "\n POLARITY=POSITIVE\n}\n";
    writeFile(dir + "/matrix/matrix", m);
    auto step = [&](const std::string& st, double x) {
        std::string base = dir + "/steps/" + st;
        writeFile(base + "/stephdr", "UNITS=MM\n");
        writeFile(base + "/profile", "UNITS=MM\nS P 0\nOB 0 0 I\nOS 0 20\nOS 40 20\nOS 40 0\nOS 0 0\nOE\nSE\n");
        std::string cu = "UNITS=MM\n$0 rect1000x600\n";
        for (double px : {x, x + 5, x + 10, x + 15}) cu += "P " + std::to_string(px) + " 5 0 P 0 0\n";
        writeFile(base + "/layers/top/features", cu);
        writeFile(base + "/layers/bot/features", cu);
        auto comp = [&](const std::string& layerDir, const std::string& ref, double px) {
            writeFile(base + "/layers/" + layerDir + "/components",
                      "UNITS=MM\nCMP 0 " + std::to_string(px) + " 5 0 N " + ref + " pkg\nTOP 0 " + std::to_string(px) + " 5 0 N 0 0 1\n");
        };
        comp("comp_+_top", "T1", x);
        comp("COMP_+_Left", "L1", x + 5);      // the folder's case is not the matrix's
        comp("comp_+_right", "R1", x + 10);
        comp("comp_+_bot", "B1", x + 15);
    };
    step("pcb", 5);
    step("Flex", 7);
    step("extra", 9);
    writeFile(dir + "/steps/panel/stephdr", "UNITS=MM\nSTEP-REPEAT {\n NAME=PCB\n X=0\n Y=0\n DX=50\n DY=0\n NX=2\n NY=1\n}\n");
    return dir;
}

// 4.5: every steps/* folder is a design of its own: the matrix's STEP records by COL first, then
// folders it does not name; a step without layer data is skipped with a warning
TEST(every_step_of_a_job_is_read) {
    std::string dir = twoStepJob("two_steps");
    auto names = listOdbSteps(dir);
    CHECK_EQ(names.size(), (size_t)4);
    if (names.size() == 4) {
        CHECK_EQ(names[0], std::string("PCB"));     // COL 1, though the matrix lists it last
        CHECK_EQ(names[1], std::string("FLEX"));
        CHECK_EQ(names[2], std::string("PANEL"));
        CHECK_EQ(names[3], std::string("extra"));
    }
    std::vector<OdbDesign> designs;
    std::string err;
    CHECK(parseOdbJob(dir, designs, err));
    CHECK_EQ(designs.size(), (size_t)4);   // PANEL has no layer data but places PCB: it is read
    if (designs.size() != 4) return;
    CHECK_EQ(designs[0].stepName, std::string("PCB"));
    CHECK_EQ(designs[1].stepName, std::string("FLEX"));
    CHECK_EQ(designs[2].stepName, std::string("PANEL"));
    CHECK_EQ(designs[3].stepName, std::string("extra"));
    CHECK_EQ(designs[2].stepHeader.repeats.size(), (size_t)1);
    CHECK_NEAR(designs[0].layerFeatures.at("top").pads[0].x, 5, 1e-9);
    CHECK_NEAR(designs[1].layerFeatures.at("top").pads[0].x, 7, 1e-9);
    CHECK_EQ(designs[1].steps.size(), (size_t)3);   // the matrix is shared
    for (auto& l : designs[0].report) CHECK(l.find("was not converted") == std::string::npos);

    // the first step is what the one-step calls read, and a step can be asked for by name
    OdbDesign first, flex, none;
    CHECK(parseOdbDirectory(dir, first));
    CHECK_EQ(first.stepName, std::string("PCB"));
    CHECK(parseOdbStep(dir, "flex", flex, err));
    CHECK_EQ(flex.stepName, std::string("FLEX"));
    CHECK_NEAR(flex.layerFeatures.at("top").pads[0].x, 7, 1e-9);
    CHECK(!parseOdbStep(dir, "nope", none, err));
    CHECK(err.find("no-step: the job has no step 'nope' (it has: PCB, FLEX, PANEL, extra)") == 0);
    OdbDesign panel;
    CHECK(parseOdbStep(dir, "PANEL", panel, err));
    // a step with neither layer data nor placed steps still fails
    OdbDesign blank;
    writeFile(dir + "/steps/blank/stephdr", "UNITS=MM\n");
    CHECK(!parseOdbStep(dir, "blank", blank, err));
    CHECK(err.find("no-layers:") == 0);

    // the library: one step by default, all of them on request, with a file name per step
    ConvertResult one = convert(dir);
    CHECK(one.success);
    CHECK_EQ(one.step, std::string("PCB"));
    CHECK_EQ(one.stepNames.size(), (size_t)5);   // "blank" (no layers, no placements) is listed, not converted
    CHECK(one.steps.empty());
    ConvertOptions all;
    all.allSteps = true;
    ConvertResult every = convert(dir, all);
    CHECK(every.success);
    CHECK_EQ(every.steps.size(), (size_t)4);   // PCB FLEX PANEL extra ("blank" is left out)
    CHECK_EQ(every.kicad_pcb, one.kicad_pcb);
    if (every.steps.size() == 4) {
        CHECK_EQ(every.steps[0].kicad_pcb, one.kicad_pcb);
        CHECK(every.steps[1].kicad_pcb != one.kicad_pcb);
        CHECK_EQ(stepFileName(every.jobName, every.steps[1].step), std::string("demo_job__FLEX.kicad_pcb"));
    }
    CHECK_EQ(stepFileName("", "../a b"), std::string("job___a_b.kicad_pcb"));
    ConvertOptions byName;
    byName.step = "Flex";
    ConvertResult f = convert(dir, byName);
    CHECK(f.success);
    if (every.steps.size() == 4) CHECK_EQ(f.kicad_pcb, every.steps[1].kicad_pcb);
}

// STEP-REPEAT blocks: every field, in the stephdr's units, moved with the panel's origin
TEST(stephdr_step_repeat_blocks_are_read) {
    std::string dir = tempDir("step_repeat_fields");
    writeFile(dir + "/stephdr", "UNITS=INCH\nX_DATUM=0\nY_DATUM=0\nSTEP-REPEAT {\n NAME=pcb\n X=1\n Y=2\n DX=0.5\n DY=0.25\n"
                                " NX=3\n NY=2\n ANGLE=90\n MIRROR=NO\n FLIP=NO\n}\nSTEP-REPEAT {\n NAME=coupon\n X=0\n Y=0\n"
                                " DX=0\n DY=0\n NX=1\n NY=1\n ANGLE=0\n MIRROR=YES\n FLIP=YES\n}\nX_ORIGIN=0.1\n");
    OdbStepHeader h;
    CHECK(parseStepHeader(dir + "/stephdr", h));
    CHECK_EQ(h.stepRepeats, 2);
    CHECK_EQ(h.repeats.size(), (size_t)2);
    if (h.repeats.size() != 2) return;
    CHECK_EQ(h.repeats[0].name, std::string("pcb"));
    CHECK_NEAR(h.repeats[0].x, 1, 1e-12);
    CHECK_NEAR(h.repeats[0].dy, 0.25, 1e-12);
    CHECK_EQ(h.repeats[0].nx, 3);
    CHECK_EQ(h.repeats[0].ny, 2);
    CHECK_NEAR(h.repeats[0].angle, 90, 1e-12);
    CHECK(!h.repeats[0].mirror && !h.repeats[0].flip);
    CHECK(h.repeats[1].mirror && h.repeats[1].flip);
    CHECK_NEAR(h.xOrigin, 0.1, 1e-12);    // a key after the block is the header's again
}

// A board placed in a panel: footprints move and turn (their pads with them), tracks, vias,
// zones and graphics move, nets get the copy's prefix, footprints new uuids; a rectangle turned
// off the axes becomes a polygon; an Edge.Cuts item goes to the edge layer asked for
TEST(a_board_is_placed_in_a_panel) {
    KicadPcb board;
    board.layers = {{0, "F.Cu", "signal", ""}, {44, "Edge.Cuts", "user", ""}};
    board.nets = {{0, ""}, {1, "GND"}};
    KicadFootprint fp;
    fp.uuid = "x";
    fp.x = 7; fp.y = -1; fp.angle = 30;
    KicadPad pad;
    pad.x = 1; pad.angle = 30; pad.netId = 1; pad.netName = "GND";
    fp.pads.push_back(pad);
    board.footprints = {fp, fp};
    board.segments.push_back({0, 0, 7, -1, 0.2, "F.Cu", 1});
    board.vias.push_back({7, -1, 0.6, {}, 0.3, "F.Cu", "B.Cu", false, 1});
    board.grRects.push_back({0, 0, 10, -5, 0.1, "default", "Edge.Cuts"});
    KicadZone z;
    z.netId = 1; z.netName = "GND"; z.layer = "F.Cu"; z.outlinePoints = {{0, 0}, {7, -1}, {0, -1}};
    board.zones.push_back(z);

    KicadPcb panel;
    panel.layers = {{0, "F.Cu", "signal", ""}, {44, "Edge.Cuts", "user", ""}};
    panel.nets = {{0, ""}, {1, "FRAME"}};
    PanelPlacement at;
    at.pivotX = 0; at.pivotY = 0; at.atX = 100; at.atY = -50; at.angle = 90; at.instance = 2;
    at.netPrefix = "Board_2-"; at.edgeLayer = "Dwgs.User";
    placeBoardInPanel(panel, board, at);
    // 90 deg clockwise on screen (y down): (x, y) -> (-y, x)
    CHECK_EQ(panel.footprints.size(), (size_t)2);
    if (panel.footprints.size() != 2) return;
    auto& f = panel.footprints[0];
    CHECK_NEAR(f.x, 101, 1e-9);
    CHECK_NEAR(f.y, -43, 1e-9);
    CHECK_NEAR(f.angle, -60, 1e-9);            // KiCad angles turn the other way
    CHECK_NEAR(f.pads[0].angle, -60, 1e-9);
    CHECK_NEAR(f.pads[0].x, 1, 1e-9);          // footprint frame: unchanged
    CHECK_EQ(f.pads[0].netId, 2);
    CHECK_EQ(f.pads[0].netName, std::string("Board_2-GND"));
    CHECK(f.uuid != "x" && f.uuid != panel.footprints[1].uuid);
    CHECK_EQ(panel.nets.size(), (size_t)3);
    if (panel.nets.size() == 3) CHECK_EQ(panel.nets[2].name, std::string("Board_2-GND"));
    CHECK_NEAR(panel.segments[0].x1, 100, 1e-9);
    CHECK_NEAR(panel.segments[0].y2, -43, 1e-9);
    CHECK_EQ(panel.segments[0].netId, 2);
    CHECK_NEAR(panel.vias[0].x, 101, 1e-9);
    CHECK_EQ(panel.zones[0].netName, std::string("Board_2-GND"));
    CHECK_NEAR(panel.zones[0].outlinePoints[1].first, 101, 1e-9);
    CHECK_EQ(panel.grRects.size(), (size_t)1);  // 90 deg keeps a rectangle a rectangle
    if (!panel.grRects.empty()) {
        CHECK_EQ(panel.grRects[0].layer, std::string("Dwgs.User"));
        CHECK_NEAR(panel.grRects[0].x2, 105, 1e-9);
        CHECK_NEAR(panel.grRects[0].y2, -40, 1e-9);
    }
    bool dwgs = false;
    for (auto& l : panel.layers) dwgs = dwgs || l.canonicalName == "Dwgs.User";
    CHECK(dwgs);
    at.angle = 45; at.instance = 3; at.netPrefix = "Board_3-"; at.edgeLayer = "Edge.Cuts";
    placeBoardInPanel(panel, board, at);
    CHECK_EQ(panel.grRects.size(), (size_t)1);
    CHECK_EQ(panel.grPolys.size(), (size_t)1);
    if (!panel.grPolys.empty()) {
        CHECK_EQ(panel.grPolys[0].pts.size(), (size_t)4);
        CHECK(!panel.grPolys[0].filled);
        CHECK_EQ(panel.grPolys[0].layer, std::string("Edge.Cuts"));
    }
    CHECK_EQ(panel.nets.size(), (size_t)4);
    CHECK(panel.footprints[2].uuid != panel.footprints[0].uuid);
}

// A panel step converts to one board: the named step NX x NY times, each copy turned ANGLE about
// the placed step's datum and its nets prefixed Board_<n>-; a panel inside a panel places its
// copies again; a flipped or mirrored copy, a step that places itself and a step the job has not
// got are reported and left out
TEST(panel_steps_place_their_steps) {
    const std::string comps = "UNITS=MM\nCMP 0 10 5 0 N R1 pkg\nTOP 0 10 5 0 N 0 0 1\n";
    const std::string netlist = "UNITS=MM\n$0 N1\n#\n#Netlist points\n#\n0 0.5 10 5 T 1 1 e e s\n";
    std::string dir = tinyJob("panel_place", "UNITS=MM\nX_DATUM=3\nY_DATUM=4\n",
                              {{"steps/pcb/layers/comp_+_top/components", comps}, {"steps/pcb/netlists/cadnet/netlist", netlist},
                               {"steps/panel/stephdr", "UNITS=MM\nSTEP-REPEAT {\n NAME=PCB\n X=100\n Y=50\n DX=40\n DY=30\n NX=2\n NY=2\n"
                                                       " ANGLE=90\n MIRROR=NO\n}\nSTEP-REPEAT {\n NAME=PCB\n X=0\n Y=0\n NX=1\n NY=1\n"
                                                       " MIRROR=YES\n FLIP=YES\n}\nSTEP-REPEAT {\n NAME=panel\n NX=1\n NY=1\n}\n"
                                                       "STEP-REPEAT {\n NAME=nope\n NX=1\n NY=1\n}\n"},
                               {"steps/outer/stephdr", "UNITS=MM\nSTEP-REPEAT {\n NAME=PANEL\n X=0\n Y=0\n NX=1\n NY=1\n ANGLE=0\n}\n"}});
    ConvertOptions o;
    o.step = "panel";
    ConvertResult r = convert(dir, o);
    CHECK(r.success);
    auto has = [&](const std::string& text, const std::string& what) { return text.find(what) != std::string::npos; };
    auto count = [](const std::string& text, const std::string& what) {
        int n = 0;
        for (size_t at = text.find(what); at != std::string::npos; at = text.find(what, at + 1)) n++;
        return n;
    };
    // R1 at (10, 5), datum (3, 4): (7, 1) from it; 90 deg clockwise -> (1, -7); copy (i, j) at
    // (100 + 40 i, 50 + 30 j): KiCad (101 + 40 i, -(43 + 30 j)), turned -90
    CHECK(has(r.kicad_pcb, "(at 101 -43 -90)"));
    CHECK(has(r.kicad_pcb, "(at 141 -43 -90)"));
    CHECK(has(r.kicad_pcb, "(at 101 -73 -90)"));
    CHECK(has(r.kicad_pcb, "(at 141 -73 -90)"));
    CHECK_EQ(count(r.kicad_pcb, "(footprint "), 4);
    for (int n = 1; n <= 4; n++) CHECK(has(r.kicad_pcb, "\"Board_" + std::to_string(n) + "-N1\""));
    CHECK(!has(r.kicad_pcb, "Board_5-"));
    CHECK_EQ(count(r.kicad_pcb, "(layer \"Edge.Cuts\")"), 4 * count(convert(dir).kicad_pcb, "(layer \"Edge.Cuts\")"));
    std::string rep;
    for (auto& l : r.report) rep += l + "\n";
    CHECK(has(rep, "info: panel panel: step PCB placed 2 x 2 at (100, 50) mm, every (40, 30) mm, turned 90 deg (copies 1-4;"));
    CHECK(has(rep, "warning: STEP-REPEAT of step PCB is flipped (bottom side up): not drawn"));
    CHECK(has(rep, "warning: STEP-REPEAT of step panel places a step inside itself: not drawn"));
    CHECK(has(rep, "warning: STEP-REPEAT of step nope cannot be read (no-step:"));

    ConvertOptions outer;
    outer.step = "outer";
    ConvertResult n = convert(dir, outer);
    CHECK(n.success);
    CHECK(has(n.kicad_pcb, "(at 101 -43 -90)"));
    CHECK(has(n.kicad_pcb, "\"Board_1-Board_4-N1\""));
    CHECK_EQ(count(n.kicad_pcb, "(footprint "), 4);

    // as before, on request: the panel without its steps (here, nothing but the warning)
    ConvertOptions off = o;
    off.placeSteps = false;
    ConvertResult bare = convert(dir, off);
    CHECK(bare.success);
    CHECK_EQ(count(bare.kicad_pcb, "(footprint "), 0);
    std::string bareRep;
    for (auto& l : bare.report) bareRep += l + "\n";
    CHECK(has(bareRep, "nested steps are not drawn"));
}

// 4.5: components come from every COMPONENT row of the matrix, not only comp_+_top / comp_+_bot:
// a row above the first copper row mounts on the top, one below it on the bottom
TEST(components_come_from_every_component_row) {
    OdbDesign d;
    CHECK(parseOdbDirectory(twoStepJob("comp_sides"), d));
    CHECK_EQ(d.topComponents.components.size(), (size_t)1);
    CHECK_EQ(d.botComponents.components.size(), (size_t)1);
    CHECK_EQ(d.otherComponents.size(), (size_t)2);
    if (d.otherComponents.size() != 2) return;
    CHECK_EQ(d.otherComponents[0].name, std::string("comp_+_left"));
    CHECK(!d.otherComponents[0].bottom);
    CHECK_EQ(d.otherComponents[1].name, std::string("comp_+_right"));
    CHECK(d.otherComponents[1].bottom);
    KicadPcb pcb = transformToKicad(d);
    std::map<std::string, std::string> side;
    std::set<std::string> uuids;
    for (auto& fp : pcb.footprints) { side[fp.refdes] = fp.layer; uuids.insert(fp.uuid); }
    CHECK_EQ(side["T1"], std::string("F.Cu"));
    CHECK_EQ(side["L1"], std::string("F.Cu"));
    CHECK_EQ(side["R1"], std::string("B.Cu"));
    CHECK_EQ(side["B1"], std::string("B.Cu"));
    CHECK_EQ(uuids.size(), pcb.footprints.size());
    // their pads are the copper flashes under them, so none is left over as a free pad
    CHECK(freePads(pcb).empty());
    auto* l1 = findPad(pcb, "L1", "1");
    CHECK(l1 && l1->shape == "rect");
}

// 4.6: the CT ... CE / RC / CR / SQ records after a PIN are that pin's outline (they used to be
// skipped); the ones before the first PIN stay the package's
TEST(pin_outlines_are_parsed) {
    std::string dir = tempDir("pin_outlines");
    writeFile(dir + "/data",
              "HDR made-up\nUNITS=MM\nLYR top bot\nNET $NONE$\nNET GND\n"
              "PKG R_0603 1.65 -1.50 -0.75 1.50 0.75;\n"
              "CT\nOB -1.47 0.72 I\nOS -1.47 0.72\nOS 1.47 0.72\nOS 1.47 -0.72\nOS -1.47 -0.72\nOS -1.47 0.72\nOE\nCE\n"
              "PIN 1 S -0.82 0.0 0 E S\n"
              "CT\nOB -1.22 -0.27 I\nOS -1.22 -0.27\nOS -1.22 0.27\nOS -0.42 0.27\nOS -0.42 -0.27\nOS -1.22 -0.27\nOE\nCE\n"
              "PIN 2 S 0.82 0.0 0 E S\n"
              "RC 0.42 -0.27 0.8 0.54\n"
              "PIN 3 T 0 1.0 0 E T\n"
              "CR 0 1.0 0.3\n"
              "PIN 4 S 0 -1.0 0 E S\n"
              "CT\nOB -0.2 -1.2 I\nOS 0.2 -1.2\nOC 0.2 -0.8 0.2 -1.0 N\nOS -0.2 -0.8\nOS -0.2 -1.2\nOE\n"
              "OB -0.05 -1.05 H\nOS 0.05 -1.05\nOS 0.05 -0.95\nOS -0.05 -1.05\nOE\nCE\n"
              "PIN 5 S 2 2 0 E S\n"
              "PKG NEXT 1 -1 -1 1 1;\nSQ 0 0 1\nPIN 1 S 0 0 0 E S\n");
    OdbEdaData eda;
    CHECK(parseEdaData(dir + "/data", eda));
    CHECK_EQ(eda.packages.size(), (size_t)2);
    if (eda.packages.size() != 2) return;
    auto& pkg = eda.packages[0];
    CHECK_EQ(pkg.outlines.size(), (size_t)1);          // the package's own outline, and only that
    CHECK_EQ(pkg.pins.size(), (size_t)5);
    if (pkg.pins.size() != 5) return;
    CHECK_EQ(pkg.pins[0].outlines.size(), (size_t)1);
    CHECK_NEAR(pkg.pins[0].outlines[0][0].first, -1.22, 1e-12);   // package frame, not pin-relative
    CHECK_EQ(pkg.pins[1].outlines.size(), (size_t)1);  // RC
    CHECK_EQ(pkg.pins[1].outlines[0].size(), (size_t)4);
    CHECK_NEAR(pkg.pins[1].outlines[0][2].first, 1.22, 1e-12);
    CHECK_EQ(pkg.pins[2].outlines.size(), (size_t)1);  // CR: a circle of radius 0.3 about the pin
    for (auto& [x, y] : pkg.pins[2].outlines[0]) CHECK_NEAR(std::hypot(x, y - 1.0), 0.3, 1e-9);
    CHECK_EQ(pkg.pins[3].outlines.size(), (size_t)1);  // the island with its arc; the hole is not an outline
    CHECK(pkg.pins[3].outlines[0].size() > 5);
    CHECK(pkg.pins[4].outlines.empty());
    CHECK_EQ(eda.packages[1].outlines.size(), (size_t)1);
    CHECK(eda.packages[1].pins[0].outlines.empty());
}

// 4.6: a pin with no copper flash and no surface of its own takes its pad shape from its pin
// outline (a centred rectangle is a rect pad, anything else a custom pad); a pin that has a flash
// keeps the flash, whatever its outline says
TEST(pin_outline_is_the_pad_shape_without_a_flash) {
    OdbDesign d = kicadDesign();
    d.edaData.units = "MM";
    OdbEdaPackage pkg;
    pkg.name = "PKG";
    auto pin = [&](double x, double y, std::vector<std::pair<double,double>> ring) {
        OdbEdaPin p;
        p.id = std::to_string(pkg.pins.size() + 1);
        p.x = x; p.y = y;
        if (!ring.empty()) p.outlines.push_back(ring);
        pkg.pins.push_back(p);
    };
    // 0: a 0.8 x 0.54 rectangle about the pin, written KiCad's way (the first point twice, closed)
    pin(-0.82, 0, {{-1.22, -0.27}, {-1.22, -0.27}, {-1.22, 0.27}, {-0.42, 0.27}, {-0.42, -0.27}, {-1.22, -0.27}});
    // 1: the same land, but this pin has a 1.0 x 0.6 flash: the flash is the pad
    pin(0.82, 0, {{0.42, -0.27}, {0.42, 0.27}, {1.22, 0.27}, {1.22, -0.27}});
    // 2: a triangle: no KiCad pad shape
    pin(0, 1.0, {{-0.3, 0.8}, {0.3, 0.8}, {0, 1.4}});
    // 3: a rectangle that is not centred on its pin
    pin(0, -1.0, {{-0.2, -1.1}, {0.6, -1.1}, {0.6, -0.9}, {-0.2, -0.9}});
    // 4: no outline at all
    pin(2, 2, {});
    d.edaData.packages = {pkg};

    // R1 as the package lies; R2 turned 90 deg clockwise (its terminals say so too)
    auto& r1 = component(d, "R1", 10, -10, 1);
    r1.pkgIdx = 0;
    for (auto& p : pkg.pins) terminal(r1, p.id, 10 + p.x, -10 + p.y, 1);
    auto& r2 = component(d, "R2", 30, -10, 1, false, 90);
    r2.pkgIdx = 0;
    for (auto& p : pkg.pins) terminal(r2, p.id, 30 + p.y, -10 - p.x, 1, 90);   // (x, y) -> (y, -x)
    flash(d, "f.cu", 10.82, -10, "rect1000x600", OdbSymbol::RECT, 1.0, 0.6);
    flash(d, "b.cu", 50, -10, "r500", OdbSymbol::ROUND, 0.5);   // the layer exists
    // The land is drawn in copper (a 1.4 mm dot over each terminal), as a CAD without pad flashes does
    for (auto& c : d.topComponents.components)
        for (auto& t : c.terminals) stroke(d, "f.cu", t.x, t.y, t.x, t.y, 1.4);

    KicadPcb pcb = transformToKicad(d);
    for (const char* ref : {"R1", "R2"}) {
        auto* p1 = findPad(pcb, ref, "1");
        CHECK(p1);
        if (!p1) continue;
        CHECK_EQ(p1->shape, std::string("rect"));
        CHECK_NEAR(p1->width, 0.8, 1e-6);       // in the pad's own frame, turned or not
        CHECK_NEAR(p1->height, 0.54, 1e-6);
        CHECK_EQ(p1->type, std::string("smd"));
        CHECK(hasLayer(*p1, "F.Cu"));
        auto* p3 = findPad(pcb, ref, "3");
        CHECK(p3 && p3->shape == "custom");
        if (p3 && !p3->primitives.empty()) {
            CHECK_EQ(p3->primitives.size(), (size_t)1);
            CHECK_EQ(p3->primitives[0].size(), (size_t)3);
            CHECK_NEAR(std::abs(signedArea2(p3->primitives[0])), 0.6 * 0.6, 1e-6);   // twice the triangle's area
        }
        auto* p4 = findPad(pcb, ref, "4");
        CHECK(p4 && p4->shape == "custom");     // off-centre: KiCad's rect pad is centred
        auto* p5 = findPad(pcb, ref, "5");
        CHECK(p5 && p5->shape == "roundrect");  // nothing to go by: the placeholder, as before
        if (p5) CHECK_NEAR(p5->width, 0.1, 1e-9);
    }
    auto* withFlash = findPad(pcb, "R1", "2");
    CHECK(withFlash);
    if (withFlash) {
        CHECK_EQ(withFlash->shape, std::string("rect"));
        CHECK_NEAR(withFlash->width, 1.0, 1e-9);
        CHECK_NEAR(withFlash->height, 0.6, 1e-9);
    }
    // R1's triangle in the pad frame (KiCad, y down): the apex points up, 0.4 above the pin
    if (auto* p3 = findPad(pcb, "R1", "3")) {
        double minY = 1e9;
        for (auto& [x, y] : p3->primitives[0]) minY = std::min(minY, y);
        CHECK_NEAR(minY, -0.4, 1e-6);
    }
    // R2 is turned a quarter clockwise: its pad 1 (package x = -0.82) sits above its origin
    const KicadFootprint* f2 = findFp(pcb, "R2");
    CHECK(f2);
    if (f2) {
        auto* p1 = findPad(pcb, "R2", "1");
        auto abs = kicadAbs(*f2, *p1);
        CHECK_NEAR(abs.first, 30, 1e-6);
        CHECK_NEAR(abs.second, 10 - 0.82, 1e-6);
        CHECK_NEAR(p1->angle, 270, 1e-9);
    }
}

// a pin with no copper flash is a land only when it is copper. KiCad lists its
// paste-only and mask-only pads (and pads on no copper at all) as pins with an outline; their
// outline must not become copper. Each pin below has the same 0.8 x 0.54 rectangle about it.
TEST(pin_outline_is_not_copper_for_paste_or_mask_only_pins) {
    OdbDesign d = kicadDesign();
    d.edaData.units = "MM";
    d.edaData.layers = {"f.cu"};
    OdbEdaPackage pkg;
    pkg.name = "PKG";
    for (int i = 0; i < 7; i++) {
        OdbEdaPin p;
        p.id = std::to_string(i + 1);
        p.x = 2.0 * i; p.y = 0;
        p.outlines.push_back({{p.x - 0.4, -0.27}, {p.x - 0.4, 0.27}, {p.x + 0.4, 0.27}, {p.x + 0.4, -0.27}});
        pkg.pins.push_back(p);
    }
    d.edaData.packages = {pkg};
    auto& u1 = component(d, "U1", 0, 0, 1);
    u1.pkgIdx = 0;
    // 1: paste opening only    2: mask opening only    3: on no net, over copper
    // 4: over copper of another net (FID)    5: nothing on copper
    // 6: only partly over copper (a 0.2 mm track through it)    7: covered by copper of its own net
    int nets[7] = {1, 1, 0, 1, 1, 1, 1};
    for (int i = 0; i < 7; i++) terminal(u1, pkg.pins[i].id, 2.0 * i, 0, nets[i]);
    flash(d, "f.paste", 0, 0, "rect300x360", OdbSymbol::RECT, 0.3, 0.36);
    flash(d, "f.mask", 2, 0, "rect500x500", OdbSymbol::RECT, 0.5, 0.5);
    stroke(d, "f.cu", 4, 0, 4, 0, 1.4);
    stroke(d, "f.cu", 6, 0, 6, 0, 1.4, 2);
    stroke(d, "f.cu", 10, 0, 10, 3, 0.2, 1);
    stroke(d, "f.cu", 12, 0, 12, 0, 1.4, 1);
    flash(d, "b.cu", 50, -10, "r500", OdbSymbol::ROUND, 0.5);   // the layer exists

    KicadPcb pcb = transformToKicad(d);
    for (const char* pin : {"1", "2", "3", "4", "5", "6"}) {
        auto* p = findPad(pcb, "U1", pin);
        CHECK(p);
        if (!p) continue;
        CHECK_EQ(p->shape, std::string("roundrect"));   // the 0.3.1 placeholder, no copper land
        CHECK_NEAR(p->width, 0.1, 1e-9);
        CHECK_NEAR(p->height, 0.1, 1e-9);
        CHECK(hasLayer(*p, "F.Cu"));                    // on its own side
    }
    if (auto* p1 = findPad(pcb, "U1", "1")) CHECK(!hasLayer(*p1, "B.Paste"));
    auto* p7 = findPad(pcb, "U1", "7");
    CHECK(p7 && p7->shape == "rect");
    if (p7) {
        CHECK_NEAR(p7->width, 0.8, 1e-6);
        CHECK_NEAR(p7->height, 0.54, 1e-6);
    }
}

// A small font in the standard font's layout: a 0.302 cell, characters drawn 0.2 wide and 0.3
// tall from (-0.1, -0.1), 0.012 lines. L is two strokes, I one, a dot is a zero-length stroke,
// 'g' hangs below the cell, '-' is drawn negative, and 'x' has no strokes at all.
static const char* kTinyFont =
    "XSIZE  0.302000\nYSIZE  0.302000\nOFFSET 0.000000\n\n"
    "CHAR L\nLINE -0.100000 0.200000 -0.100000 -0.100000 P R 0.012000\nLINE -0.100000 -0.100000 0.100000 -0.100000 P R 0.012000\nECHAR\n\n"
    "CHAR I\nLINE 0.000000 -0.100000 0.000000 0.200000 P R 0.012000\nECHAR\n\n"
    "CHAR .\nLINE 0.025000 -0.075000 0.025000 -0.075000 P R 0.012000\nECHAR\n\n"
    "CHAR g\nLINE -0.100000 -0.150000 0.100000 -0.150000 P R 0.012000\nECHAR\n\n"
    "CHAR -\nLINE -0.100000 0.050000 0.100000 0.050000 N S 0.024000\nECHAR\n\n"
    "CHAR x\nECHAR\n";

// 4.7: fonts/<name>: XSIZE / YSIZE / OFFSET, then CHAR <c> LINE ... ECHAR per character
TEST(font_file_is_parsed) {
    std::string dir = tempDir("font");
    writeFile(dir + "/standard", kTinyFont);
    OdbFont f;
    CHECK(parseFont(dir + "/standard", f));
    CHECK_NEAR(f.xsize, 0.302, 1e-12);
    CHECK_NEAR(f.ysize, 0.302, 1e-12);
    CHECK_NEAR(f.offset, 0.0, 1e-12);
    CHECK_EQ(f.glyphs.size(), (size_t)6);
    CHECK_EQ(f.glyphs['L'].size(), (size_t)2);       // a multi-stroke character
    CHECK_EQ(f.glyphs['I'].size(), (size_t)1);
    CHECK(f.glyphs.count('x') == 1 && f.glyphs['x'].empty());
    CHECK_NEAR(f.glyphs['L'][1].x2, 0.1, 1e-12);
    CHECK_NEAR(f.glyphs['L'][0].width, 0.012, 1e-12);
    CHECK(f.glyphs['L'][0].positive && f.glyphs['L'][0].round);
    CHECK(!f.glyphs['-'][0].positive && !f.glyphs['-'][0].round);
    CHECK_NEAR(f.glyphs['-'][0].width, 0.024, 1e-12);
    CHECK(f.polygons.empty());
    CHECK_EQ(f.units, std::string(""));              // no UNITS line: inches
    OdbFont none;
    CHECK(!parseFont(dir + "/absent", none));
    writeFile(dir + "/empty", "");
    CHECK(!parseFont(dir + "/empty", none));         // no cell size: not a font
}

// 4.7: T x y font pol orient [angle] xsize ysize width 'text' version;attrs (they used to be
// counted and dropped); V is the same record drawn downwards
TEST(text_records_are_parsed) {
    std::string dir = tempDir("text_records");
    writeFile(dir + "/features",
              "UNITS=INCH\n$0 r10\n@0 .string\n&0 TEXT_A\n"
              "L 0 0 1 0 0 P 0\n"
              "T 2.48232 7.9765 standard P 0 0.4 0.4 3.00000 'FS' 1;0\n"
              "T 1 2 standard N 8 30.5 0.1 0.2 1.5 'two words; and a semicolon' 0;0=0\n"
              "T 3 4 other P 6 0.05 0.06 1 'it''s' 1\n"
              "V 5 6 standard P 1 0.1 0.1 2 'DOWN' 1\n"
              "B 0 0 code128 standard P 0 E 0.01 0.2 Y N T 'X'\n"
              "P 1 1 0 P 0 0\n");
    OdbFeatures f;
    CHECK(parseFeatures(dir + "/features", f));
    CHECK_EQ(f.texts.size(), (size_t)4);
    CHECK_EQ(f.barcodes, 1);
    if (f.texts.size() != 4) return;
    auto& a = f.texts[0];
    CHECK_EQ(a.featIdx, 1);                 // texts take their place in the feature order
    CHECK_NEAR(a.x, 2.48232, 1e-12);
    CHECK_NEAR(a.y, 7.9765, 1e-12);
    CHECK_EQ(a.font, std::string("standard"));
    CHECK_EQ(a.polarity, std::string("P"));
    CHECK(!a.mirror);
    CHECK_NEAR(a.angle, 0, 1e-12);
    CHECK_NEAR(a.xsize, 0.4, 1e-12);
    CHECK_NEAR(a.ysize, 0.4, 1e-12);
    CHECK_NEAR(a.weight, 3.0, 1e-12);
    CHECK_EQ(a.text, std::string("FS"));
    CHECK_EQ(a.version, 1);
    CHECK(!a.vertical);
    CHECK(a.attrs.count(0) == 1);           // the .string attribute stays with the record
    auto& b = f.texts[1];
    CHECK_EQ(b.polarity, std::string("N"));
    CHECK_NEAR(b.angle, 30.5, 1e-12);       // orient 8 carries its angle
    CHECK_NEAR(b.xsize, 0.1, 1e-12);
    CHECK_NEAR(b.ysize, 0.2, 1e-12);
    CHECK_NEAR(b.weight, 1.5, 1e-12);
    CHECK_EQ(b.text, std::string("two words; and a semicolon"));
    CHECK_EQ(b.version, 0);
    CHECK_EQ(b.attrs[0], std::string("0"));
    auto& c = f.texts[2];
    CHECK(c.mirror);                        // orient 6: mirrored, 180 deg
    CHECK_NEAR(c.angle, 180, 1e-12);
    CHECK_EQ(c.text, std::string("it''s"));
    CHECK(f.texts[3].vertical);
    CHECK_NEAR(f.texts[3].angle, 90, 1e-12);
    CHECK_EQ(f.pads[0].featIdx, 6);         // line, 4 texts, barcode, then the pad
}

// Spec v2 3.8: a font may carry its own UNITS line, square-ended lines and filled POLYGON
// glyphs; a line that cannot be read is skipped and named
TEST(font_units_polygons_and_bad_lines) {
    std::string dir = tempDir("font_v2");
    writeFile(dir + "/mm",
              "UNITS=MM\nXSIZE 2\nYSIZE 3\nOFFSET 0\n"
              "CHAR A\nLINE 0 0 1 2 P S 0.2\nPOLYGON 4\n0 0\n1 0\n1 1\n0 1\nLINE 0 0 0 1 P R 0.1\nECHAR\n"
              "CHAR B\nPOLYGON\n0 0 2 0\n1 3\nPOLYGON 3 0 0 1 0 1 1\nECHAR\n"
              "CHAR C\nLINE 0 0 oops\nSPLINE 1 2 3\nECHAR\n"
              "stray\n");
    OdbFont f;
    std::vector<int> bad;
    CHECK(parseFont(dir + "/mm", f, &bad));
    CHECK_EQ(f.units, std::string("MM"));
    CHECK_EQ(f.glyphs['A'].size(), (size_t)2);
    CHECK(!f.glyphs['A'][0].round);                  // S: square ends
    CHECK(f.glyphs['A'][1].round);
    CHECK_EQ(f.polygons['A'].size(), (size_t)1);
    CHECK_EQ(f.polygons['A'][0].size(), (size_t)4);
    CHECK_NEAR(f.polygons['A'][0][2].first, 1, 1e-12);
    CHECK_NEAR(f.polygons['A'][0][2].second, 1, 1e-12);
    CHECK_EQ(f.polygons['B'].size(), (size_t)2);     // points on several lines, or on the POLYGON line
    CHECK_EQ(f.polygons['B'][0].size(), (size_t)3);
    CHECK_EQ(f.polygons['B'][1].size(), (size_t)3);
    CHECK(f.glyphs.count('B') == 1 && f.glyphs['B'].empty());
    CHECK_EQ(bad.size(), (size_t)3);                 // the broken LINE, the unknown record, the stray line
    if (bad.size() == 3) { CHECK_EQ(bad[0], 21); CHECK_EQ(bad[1], 22); CHECK_EQ(bad[2], 24); }
    writeFile(dir + "/spaced", "UNITS = mm\nXSIZE 1\nYSIZE 1\n");
    OdbFont g;
    CHECK(parseFont(dir + "/spaced", g));
    CHECK_EQ(g.units, std::string("MM"));

    // drawn: the font's millimetres stay millimetres, the polygon is scaled and placed like a stroke
    OdbText t;
    t.x = 10; t.y = 20; t.polarity = "N"; t.xsize = 4; t.ysize = 3; t.weight = 2; t.text = "AB";
    std::vector<TextPolygon> polys;
    auto st = textStrokes(t, f, nullptr, &polys);
    CHECK_EQ(st.size(), (size_t)2);
    CHECK_NEAR(st[0].width, 0.4, 1e-12);             // 2 x 0.2 mm, not x 25.4
    CHECK(!st[0].round && st[1].round);
    CHECK_NEAR(st[0].x2, 10 + 1 * 2, 1e-12);         // xsize / XSIZE = 2
    CHECK_NEAR(st[0].y2, 20 + 2 * 1, 1e-12);
    CHECK_EQ(polys.size(), (size_t)3);
    if (polys.size() != 3) return;
    CHECK(!polys[0].positive);                       // a negative text clears
    CHECK_NEAR(polys[0].points[2].first, 10 + 2, 1e-12);
    CHECK_NEAR(polys[0].points[2].second, 20 + 1, 1e-12);
    CHECK_NEAR(polys[1].points[1].first, 10 + 4 + 2 * 2, 1e-12);   // B: one xsize on
    CHECK_NEAR(polys[1].points[2].second, 20 + 3, 1e-12);
}

// 4.7 / spec v2 3.8: a text is its characters' font strokes, scaled by xsize / XSIZE and
// ysize / YSIZE, each glyph's own zero one xsize on from the text's location; mirrored, turned
// clockwise; `weight` font line widths wide
TEST(text_strokes_are_scaled_and_placed) {
    std::string dir = tempDir("text_strokes");
    writeFile(dir + "/standard", kTinyFont);
    OdbFont font;
    CHECK(parseFont(dir + "/standard", font));
    OdbText t;
    t.x = 10; t.y = 20; t.font = "standard"; t.polarity = "P";
    t.xsize = 0.604; t.ysize = 0.906;       // twice the box wide, three times as tall
    t.weight = 3; t.text = "LI .q";
    std::string missing;
    auto s = textStrokes(t, font, &missing);
    CHECK_EQ(s.size(), (size_t)4);          // L 2, I 1, blank 0, dot 1, q not in the font
    CHECK_EQ(missing, std::string("q"));
    if (s.size() != 4) return;
    // L: glyph coordinates as the font writes them, from the text's location
    CHECK_NEAR(s[0].x1, 10 - 0.1 * 2, 1e-9);
    CHECK_NEAR(s[0].y1, 20 + 0.2 * 3, 1e-9);
    CHECK_NEAR(s[0].x2, 10 - 0.1 * 2, 1e-9);
    CHECK_NEAR(s[0].y2, 20 - 0.1 * 3, 1e-9);
    CHECK_NEAR(s[1].x2, 10 + 0.1 * 2, 1e-9);
    CHECK_NEAR(s[1].y2, 20 - 0.1 * 3, 1e-9);
    // I: one xsize on
    CHECK_NEAR(s[2].x1, 10 + 0.604, 1e-9);
    CHECK_NEAR(s[2].y2, 20 + 0.2 * 3, 1e-9);
    // the dot: fourth box (the blank takes one), a zero-length stroke
    CHECK_NEAR(s[3].x1, 10 + 3 * 0.604 + 0.025 * 2, 1e-9);
    CHECK_NEAR(s[3].y1, 20 - 0.075 * 3, 1e-9);
    CHECK_NEAR(s[3].x1, s[3].x2, 1e-12);
    // 3 font line widths of 0.012 in, whatever the character size
    CHECK_NEAR(s[0].width, 3 * 0.012 * 25.4, 1e-9);
    CHECK(s[0].positive);

    // turned 90 deg clockwise: the text runs down, its tops point right
    t.text = "L"; t.angle = 90;
    s = textStrokes(t, font);
    CHECK_NEAR(s[0].x1, 10 + 0.6, 1e-9);
    CHECK_NEAR(s[0].y1, 20 + 0.2, 1e-9);
    CHECK_NEAR(s[1].x2, 10 - 0.3, 1e-9);
    CHECK_NEAR(s[1].y2, 20 - 0.2, 1e-9);
    // mirrored in X, then turned: the foot of the L now runs up
    t.mirror = true;
    s = textStrokes(t, font);
    CHECK_NEAR(s[1].x2, 10 - 0.3, 1e-9);
    CHECK_NEAR(s[1].y2, 20 + 0.2, 1e-9);
    // a negative text clears; a negative font line in a negative text draws
    t.mirror = false; t.angle = 0; t.polarity = "N"; t.text = "I-";
    s = textStrokes(t, font);
    CHECK_EQ(s.size(), (size_t)2);
    CHECK(!s[0].positive);
    CHECK(s[1].positive);
    CHECK(s[0].round && !s[1].round);
    CHECK_NEAR(s[1].width, 3 * 0.024 * 25.4, 1e-9);
    // V: one ysize up per character
    t.polarity = "P"; t.vertical = true; t.text = "II";
    s = textStrokes(t, font);
    CHECK_NEAR(s[1].x1, s[0].x1, 1e-9);
    CHECK_NEAR(s[1].y1, s[0].y1 + 0.906, 1e-9);
}

// Spec v2 3.6: an arc's direction token is optional (the reference reader does not read one):
// honoured when written, counter-clockwise (centre on the left of travel) when not; polarity
// and dcode may be missing too
TEST(arc_direction_token_is_optional) {
    std::string dir = tempDir("arc_cw");
    writeFile(dir + "/features",
              "UNITS=MM\n$0 r100\n"
              "A 1 0 0 1 0 0 0 P 0 Y;0=1\n"
              "A 1 0 0 1 0 0 0 P 0 N\n"
              "A 1 0 0 1 0 0 0 P 0\n"
              "A 1 0 0 1 0 0 0 N\n"
              "A 1 0 0 1 0 0 0\n"
              "A 1 0 0 1 0 0 0 P Y\n"
              "A 1 0 0 1 0 0 0 n 0 y\n");
    OdbFeatures f;
    CHECK(parseFeatures(dir + "/features", f));
    CHECK_EQ(f.arcs.size(), (size_t)7);
    if (f.arcs.size() != 7) return;
    CHECK(f.arcs[0].clockwise && f.arcs[0].directionGiven);
    CHECK(f.arcs[0].attrs.count(0) == 1);
    CHECK(!f.arcs[1].clockwise && f.arcs[1].directionGiven);
    CHECK(!f.arcs[2].clockwise && !f.arcs[2].directionGiven);
    CHECK_EQ(f.arcs[2].polarity, std::string("P"));
    CHECK(!f.arcs[3].clockwise && !f.arcs[3].directionGiven);   // a lone N is the polarity
    CHECK_EQ(f.arcs[3].polarity, std::string("N"));
    CHECK(!f.arcs[4].directionGiven);
    CHECK_EQ(f.arcs[4].polarity, std::string("P"));
    CHECK(f.arcs[5].clockwise && f.arcs[5].directionGiven);
    CHECK(f.arcs[6].clockwise && f.arcs[6].directionGiven);
    CHECK_EQ(f.arcs[6].polarity, std::string("N"));
    CHECK_EQ(f.arcs[6].featIdx, 6);
}

// A job with a text layer: NOTES is a DOCUMENT row no KiCad layer answers to
static std::string textJob(const std::string& name, const std::string& source, const std::string& notes,
                           bool withFont = true) {
    std::string dir = tempDir(name);
    writeFile(dir + "/misc/info", "JOB_NAME=demo\nUNITS=INCH\nODB_SOURCE=" + source + "\n");
    writeFile(dir + "/matrix/matrix",
              "STEP {\n COL=1\n NAME=PCB\n}\n"
              "LAYER {\n ROW=1\n CONTEXT=BOARD\n TYPE=SILK_SCREEN\n NAME=SST\n POLARITY=POSITIVE\n}\n"
              "LAYER {\n ROW=2\n CONTEXT=BOARD\n TYPE=SIGNAL\n NAME=TOP\n POLARITY=POSITIVE\n}\n"
              "LAYER {\n ROW=3\n CONTEXT=BOARD\n TYPE=SIGNAL\n NAME=BOT\n POLARITY=POSITIVE\n}\n"
              "LAYER {\n ROW=4\n CONTEXT=MISC\n TYPE=DOCUMENT\n NAME=EMPTY_DOC\n POLARITY=POSITIVE\n}\n"
              "LAYER {\n ROW=5\n CONTEXT=MISC\n TYPE=DOCUMENT\n NAME=USER.1\n POLARITY=POSITIVE\n}\n"
              "LAYER {\n ROW=6\n CONTEXT=MISC\n TYPE=DOCUMENT\n NAME=NOTES\n POLARITY=POSITIVE\n}\n"
              "LAYER {\n ROW=7\n CONTEXT=MISC\n TYPE=DOCUMENT\n NAME=BIG\n POLARITY=POSITIVE\n}\n");
    writeFile(dir + "/steps/pcb/profile", "S P 0\nOB 0 0 I\nOS 0 2\nOS 3 2\nOS 3 0\nOS 0 0\nOE\nSE\n");
    writeFile(dir + "/steps/pcb/layers/top/features", "$0 r50\nP 0.5 0.5 0 P 0 0\n");
    writeFile(dir + "/steps/pcb/layers/bot/features", "$0 r50\nP 0.5 0.5 0 P 0 0\n");
    writeFile(dir + "/steps/pcb/layers/sst/features", "$0 r10\nT 1 1 standard P 0 0.1 0.1 1 'LI' 1\n");
    writeFile(dir + "/steps/pcb/layers/empty_doc/features", "$0 r10\n");
    writeFile(dir + "/steps/pcb/layers/user.1/features", "$0 r10\nL 0 0 1 1 0 P 0\n");
    writeFile(dir + "/steps/pcb/layers/notes/features", notes);
    std::string big = "$0 r10\n";
    for (int i = 0; i < 30; i++) big += "L 0 " + std::to_string(i * 0.01) + " 1 " + std::to_string(i * 0.01) + " 0 P 0\n";
    writeFile(dir + "/steps/pcb/layers/big/features", big);
    if (withFont) writeFile(dir + "/fonts/Standard", kTinyFont);   // found whatever its case
    return dir;
}

static int linesOn(const KicadPcb& pcb, const std::string& layer) {
    int n = 0;
    for (auto& l : pcb.grLines) if (l.layer == layer) n++;
    return n;
}

// 4.7: texts reach the board as gr_line strokes on their layer; a DOCUMENT layer without a KiCad
// layer of its name is drawn on the next free User.N layer under its own name
TEST(texts_are_drawn_as_strokes_on_their_layer) {
    const std::string notes = "$0 r10\nL 0 0 1 0 0 P 0\nT 2.5 1 standard P 0 0.4 0.4 3.00000 'LI.' 1;0\n"
                              "T 1 1.5 standard P 8 90 0.1 0.1 1 '$$JOB $$layer' 1\n";
    OdbDesign d;
    CHECK(parseOdbDirectory(textJob("text_layers", "made-up", notes), d));
    auto& f = d.layerFeatures.at("notes");
    CHECK_EQ(f.texts.size(), (size_t)2);
    CHECK_EQ(f.texts[0].strokes, 4);
    CHECK_EQ(f.texts[1].text, std::string("demo notes"));   // dynamic text is filled in
    CHECK_EQ(f.texts[1].strokes, 0);                        // (the tiny font has none of those letters)
    CHECK_EQ(f.lines.size(), (size_t)5);                    // its own line + 4 strokes
    int fromText = 0;
    for (auto& l : f.lines) if (l.textIdx == 0) { fromText++; CHECK_EQ(l.featIdx, f.texts[0].featIdx); }
    CHECK_EQ(fromText, 4);
    CHECK_NEAR(f.texts[0].x, 2.5 * 25.4, 1e-9);             // the record itself is in mm too
    CHECK_NEAR(f.texts[0].xsize, 0.4 * 25.4, 1e-9);
    bool said = false, glyphs = false;
    for (auto& l : d.report) {
        said = said || l == "info: layer notes: 1 text record(s) drawn as 4 font strokes";
        glyphs = glyphs || l.find("warning: layer notes: the font has no character for") == 0;
    }
    CHECK(said);
    CHECK(glyphs);

    KicadPcb pcb = transformToKicad(d);
    // SST is the top silkscreen; USER.1 is taken by name, so NOTES goes to User.2 and BIG to User.3
    CHECK_EQ(linesOn(pcb, "F.SilkS"), 3);
    CHECK_EQ(linesOn(pcb, "User.1"), 1);
    CHECK_EQ(linesOn(pcb, "User.2"), 5);
    CHECK_EQ(linesOn(pcb, "User.3"), 30);
    std::string out = writeString(pcb);
    CHECK(out.find("(51 \"User.2\" user \"NOTES\")") != std::string::npos);
    CHECK(out.find("(52 \"User.3\" user \"BIG\")") != std::string::npos);
    CHECK(out.find("EMPTY_DOC") == std::string::npos);      // nothing on it: no layer spent
    // the first stroke of 'L' at 2.5, 1 in: the glyph's left edge (x = -0.1) from y = 0.2 down to
    // -0.1 in font units, scaled by 0.4 / 0.302, the glyph's zero on the text's location
    const double gk = 0.4 / 0.302 * 25.4;
    bool found = false;
    for (auto& l : pcb.grLines) {
        if (l.layer != "User.2" || std::abs(l.width - 3 * 0.012 * 25.4) > 1e-6) continue;
        if (std::abs(l.x2 - (63.5 - 0.1 * gk)) < 1e-6 && std::abs(l.y2 + (25.4 - 0.1 * gk)) < 1e-6) {
            found = true;
            CHECK_NEAR(l.x1, 63.5 - 0.1 * gk, 1e-6);
            CHECK_NEAR(l.y1, -(25.4 + 0.2 * gk), 1e-6);
        }
    }
    CHECK(found);

    // the limit: a document layer over it is left out, and said so
    d.documentFeatureLimit = 10;
    reportDocumentLayers(d);
    KicadPcb small = transformToKicad(d);
    CHECK_EQ(linesOn(small, "User.2"), 5);
    CHECK_EQ(linesOn(small, "User.3"), 0);
    bool warned = false;
    for (auto& l : d.report) warned = warned || l.find("warning: document layer BIG (30 features) is not drawn") == 0;
    CHECK(warned);
    d.documentFeatureLimit = 0;
    CHECK_EQ(linesOn(transformToKicad(d), "User.2"), 0);
    CHECK_EQ(linesOn(transformToKicad(d), "User.1"), 1);    // a layer KiCad has by name is not a document extra

    // a KiCad-made job: its other DOCUMENT rows (tenting, plugging...) stay out, texts are still drawn
    OdbDesign k;
    CHECK(parseOdbDirectory(textJob("text_kicad", "KiCad EDA", notes), k));
    KicadPcb kp = transformToKicad(k);
    CHECK_EQ(linesOn(kp, "User.2"), 0);
    CHECK_EQ(linesOn(kp, "F.SilkS"), 3);

    // no font in the job: the text is reported, not drawn, and nothing else is lost
    OdbDesign n;
    CHECK(parseOdbDirectory(textJob("text_nofont", "made-up", notes, false), n));
    CHECK_EQ(n.layerFeatures.at("notes").lines.size(), (size_t)1);
    bool noFont = false;
    for (auto& l : n.report) noFont = noFont || l.find("2 text record(s) not drawn, the job has no font 'standard'") != std::string::npos;
    CHECK(noFont);
}

// 4.7: a negative text clears the copper drawn before it, like any negative line
TEST(negative_text_cuts_the_plane_under_it) {
    std::string dir = textJob("text_negative", "made-up", "$0 r10\n");
    writeFile(dir + "/steps/pcb/layers/top/features",
              "$0 r50\nS P 0\nOB 0.2 0.2 I\nOS 0.2 1.8\nOS 2.8 1.8\nOS 2.8 0.2\nOS 0.2 0.2\nOE\nSE\n"
              "T 1 1 standard N 0 0.4 0.4 3 'I' 1\n");
    OdbDesign d;
    CHECK(parseOdbDirectory(dir, d));
    KicadPcb pcb = transformToKicad(d);
    // the 'I' is a vertical stroke at x = 1 in (the glyph's own zero), from 0.13 in under y = 1 in to 0.26 in over it
    const double x = 1 * 25.4, y = -(1 + 0.1) * 25.4;
    bool onPlane = false, inStroke = false;
    auto inside = [](const std::vector<std::pair<double,double>>& poly, double px, double py) {
        bool c = false;
        for (size_t i = 0, j = poly.size() - 1; i < poly.size(); j = i++)
            if ((poly[i].second > py) != (poly[j].second > py) &&
                px < (poly[j].first - poly[i].first) * (py - poly[i].second) / (poly[j].second - poly[i].second) + poly[i].first)
                c = !c;
        return c;
    };
    auto covered = [&](double px, double py) {
        for (auto& z : pcb.zones)
            for (auto& poly : z.filledPolygons) if (inside(poly, px, py)) return true;
        for (auto& g : pcb.grPolys)
            if (g.layer == "F.Cu" && inside(g.pts, px, py)) return true;
        return false;
    };
    onPlane = covered(x + 5, y);
    inStroke = covered(x, y);
    CHECK(onPlane);
    CHECK(!inStroke);
    for (auto& s : pcb.segments) CHECK(std::abs(s.width - 3 * 0.012 * 25.4) > 1e-6);  // not drawn as a track
}

// 4.8: the matrix keeps COL, OLD_NAME, COLOR and ID, and its rows their numbers (a gap is legal)
TEST(matrix_is_captured_in_full) {
    std::string dir = tempDir("matrix_full");
    writeFile(dir + "/matrix",
              "STEP {\n    COL=2\n    ID=77\n    NAME=PANEL\n}\n\nSTEP {\n    COL=1\n    NAME=REV_A\n}\n\n"
              "LAYER {\n    ROW=20\n    CONTEXT=BOARD\n    TYPE=COMPONENT\n    NAME=COMP_+_BOT\n    POLARITY=POSITIVE\n"
              "    START_NAME=\n    END_NAME=\n    OLD_NAME=\n}\n\n"
              "LAYER {\n    ROW=22\n    CONTEXT=MISC\n    TYPE=DOCUMENT\n    NAME=PROCMAP_BOT\n    POLARITY=POSITIVE\n"
              "    START_NAME=\n    END_NAME=\n    OLD_NAME=PMAP_B\n    COLOR=16711935\n    ID=4711\n}\n\n"
              "LAYER {\n    ROW=9\n    CONTEXT=BOARD\n    TYPE=POWER_GROUND\n    NAME=PG5\n    POLARITY=NEGATIVE\n}\n"
              "LAYER {\n    ROW=18\n    CONTEXT=BOARD\n    TYPE=DRILL\n    NAME=DRILL\n    POLARITY=POSITIVE\n    START_NAME=SIGT\n    END_NAME=SIGB\n}\n"
              "LAYER {\n    ROW=6\n    CONTEXT=BOARD\n    TYPE=DIELECTRIC\n    NAME=D1\n    POLARITY=POSITIVE\n    DIELECTRIC_TYPE=PREPREG\n}\n");
    std::vector<OdbMatrixStep> steps;
    std::vector<OdbLayer> layers;
    CHECK(parseMatrix(dir + "/matrix", steps, layers));
    CHECK_EQ(steps.size(), (size_t)2);
    if (steps.size() == 2) {
        CHECK_EQ(steps[0].name, std::string("PANEL"));
        CHECK_EQ(steps[0].col, 2);
        CHECK_EQ(steps[1].col, 1);
    }
    CHECK_EQ(layers.size(), (size_t)5);
    if (layers.size() != 5) return;
    CHECK_EQ(layers[0].row, 20);            // rows keep their numbers and their file order
    CHECK_EQ(layers[1].row, 22);            // 21 is skipped: nothing is renumbered
    CHECK_EQ(layers[1].oldName, std::string("PMAP_B"));
    CHECK_EQ(layers[1].name, std::string("PROCMAP_BOT"));
    CHECK_EQ(layers[1].color, std::string("16711935"));
    CHECK_EQ(layers[1].id, std::string("4711"));
    CHECK_EQ(layers[1].context, std::string("MISC"));
    CHECK(layers[0].oldName.empty() && layers[0].color.empty());
    CHECK_EQ(layers[2].polarity, std::string("NEGATIVE"));
    CHECK_EQ(layers[3].startName, std::string("SIGT"));
    CHECK_EQ(layers[3].endName, std::string("SIGB"));
    CHECK_EQ(layers[4].dielectricType, std::string("PREPREG"));
    std::string first;
    std::vector<OdbLayer> again;
    CHECK(parseMatrix(dir + "/matrix", first, again));
    CHECK_EQ(first, std::string("PANEL"));  // the first STEP record of the file
}

// 4.8: misc/info: the ODB++ version and the dates are kept, with every other key
TEST(info_captures_version_and_dates) {
    std::string dir = tempDir("info_full");
    writeFile(dir + "/info", "JOB_NAME=designodb_rigidflex\nODB_VERSION_MAJOR=8\nODB_VERSION_MINOR=1\n"
                             "ODB_SOURCE=Xpedition xPCB Layout\nCREATION_DATE=20161024.101454\nSAVE_DATE=20161025.090000\n"
                             "SAVE_APP=Valor NPI 10.2\nSAVE_USER=someone\nUNITS=INCH\nMAX_UID=544796\n");
    OdbInfo info;
    CHECK(parseInfo(dir + "/info", info));
    CHECK_EQ(info.versionMajor, 8);
    CHECK_EQ(info.versionMinor, 1);
    CHECK_EQ(info.creationDate, std::string("20161024.101454"));
    CHECK_EQ(info.saveDate, std::string("20161025.090000"));
    CHECK_EQ(info.jobName, std::string("designodb_rigidflex"));
    CHECK_EQ(info.odbSource, std::string("Xpedition xPCB Layout"));
    CHECK_EQ(info.units, std::string("INCH"));
    CHECK_EQ(info.values["MAX_UID"], std::string("544796"));   // a key the reader has no use for
    CHECK_EQ(info.values.size(), (size_t)10);
    writeFile(dir + "/old", "JOB_NAME=x\nODB_VERSION_MAJOR=\n");
    OdbInfo none;
    CHECK(parseInfo(dir + "/old", none));
    CHECK_EQ(none.versionMajor, -1);
    CHECK(none.creationDate.empty());
}

// A job whose matrix and misc/info the caller writes
static std::string matrixJob(const std::string& name, const std::string& info, const std::string& extraRows,
                             const std::string& drillSpan = "") {
    std::string dir = tempDir(name);
    if (!info.empty()) writeFile(dir + "/misc/info", info);
    writeFile(dir + "/misc/job_name", "named_by_file\n");
    auto row = [](int r, const std::string& type, const std::string& n, const std::string& more = "") {
        return "LAYER {\n ROW=" + std::to_string(r) + "\n CONTEXT=BOARD\n TYPE=" + type + "\n NAME=" + n + "\n POLARITY=POSITIVE\n" + more + "}\n";
    };
    writeFile(dir + "/matrix/matrix", "STEP {\n COL=1\n NAME=PCB\n}\n" + row(1, "SIGNAL", "TOP") + row(2, "SIGNAL", "BOT") +
                                          row(5, "DRILL", "DRL", drillSpan) + extraRows);
    writeFile(dir + "/steps/pcb/profile", "UNITS=MM\nS P 0\nOB 0 0 I\nOS 0 20\nOS 30 20\nOS 30 0\nOS 0 0\nOE\nSE\n");
    for (const char* l : {"top", "bot"})
        writeFile(dir + "/steps/pcb/layers/" + l + "/features", "UNITS=MM\n$0 r600\nP 5 5 0 P 0 0\n");
    writeFile(dir + "/steps/pcb/layers/drl/features", "UNITS=MM\n$0 r300\n@0 .drill\nP 5 5 0 P 0 0;0=2\n");
    writeFile(dir + "/steps/pcb/layers/odd/features", "UNITS=MM\n$0 r100\nL 1 1 9 1 0 P 0\n");
    return dir;
}

static bool reportHas(const std::vector<std::string>& report, const std::string& what) {
    for (auto& l : report) if (l.find(what) != std::string::npos) return true;
    return false;
}

// 4.8 / 3.2 / 3.11: the version goes into the board's generator and the report; an old job, an
// unknown layer TYPE and a drill span that names no copper row are read with a warning; an ODX
// (XML) matrix is refused
TEST(version_and_matrix_checks_are_reported) {
    const std::string oddRow = "LAYER {\n ROW=7\n CONTEXT=MISC\n TYPE=SCRIBBLE\n NAME=ODD\n POLARITY=POSITIVE\n}\n";
    {
        ConvertResult r = convert(matrixJob("ver_81", "UNITS=MM\nODB_VERSION_MAJOR=8\nODB_VERSION_MINOR=1\nODB_SOURCE=made-up\n", ""));
        CHECK(r.success);
        CHECK(r.kicad_pcb.find("(generator \"odb2kicad/" ODB2KICAD_VERSION " odb++:v8.1\")") != std::string::npos);
        CHECK(reportHas(r.report, "info: ODB++ version 8.1, source made-up"));
        CHECK(reportHas(r.report, "info: matrix: 1 step(s), 3 layer rows (1 to 5, no row 3, 4)"));
        CHECK(reportHas(r.report, "info: matrix row 1 TOP (SIGNAL) -> F.Cu"));
        CHECK(reportHas(r.report, "info: matrix row 5 DRL (DRILL) -> holes and vias"));
        CHECK(!reportHas(r.report, "warning:"));
        CHECK_EQ(r.jobName, std::string("named_by_file"));    // misc/job_name when misc/info has no JOB_NAME
    }
    {   // version 7: read, with the reference reader's warning
        ConvertResult r = convert(matrixJob("ver_7", "UNITS=MM\nODB_VERSION_MAJOR=7\nODB_VERSION_MINOR=0\n", ""));
        CHECK(r.success);
        CHECK(r.kicad_pcb.find("odb++:v7.0") != std::string::npos);
        CHECK(reportHas(r.report, "warning: the job is ODB++ version 7.0; it is read as version 8, which may lose data"));
    }
    {   // no misc/info at all (demo-design): no version in the generator, said in the report
        ConvertResult r = convert(matrixJob("ver_none", "", ""));
        CHECK(r.success);
        CHECK(r.kicad_pcb.find("(generator \"odb2kicad/" ODB2KICAD_VERSION "\")") != std::string::npos);
        CHECK(reportHas(r.report, "info: ODB++ version not given"));
    }
    {   // an unknown TYPE is a DOCUMENT layer (drawn on a free user layer), with a warning
        OdbDesign d;
        CHECK(parseOdbDirectory(matrixJob("type_unknown", "UNITS=MM\n", oddRow), d));
        CHECK(reportHas(d.report, "warning: layer ODD: unknown type SCRIBBLE is changed to DOCUMENT"));
        CHECK_EQ(d.layers.back().type, std::string("DOCUMENT"));
        CHECK_EQ(d.layers.back().writtenType, std::string("SCRIBBLE"));
        std::vector<std::string> rep;
        KicadPcb pcb = transformToKicad(d, &rep);
        CHECK_EQ(linesOn(pcb, "User.1"), 1);
        CHECK(reportHas(rep, "info: matrix row 7 ODD (SCRIBBLE read as DOCUMENT, MISC) -> User.1"));
    }
    {   // a drill span naming a layer that is not copper: a through hole, with a warning
        OdbDesign d;
        CHECK(parseOdbDirectory(matrixJob("span_bad", "UNITS=MM\n", "", " START_NAME=TOP\n END_NAME=NOWHERE\n"), d));
        CHECK(reportHas(d.report, "warning: drill layer DRL: span end 'NOWHERE' is not a copper layer of the matrix; drilled through instead"));
        KicadPcb pcb = transformToKicad(d);
        CHECK_EQ(pcb.vias.size(), (size_t)1);
        if (!pcb.vias.empty()) CHECK(!pcb.vias[0].blind);
        OdbDesign ok;
        CHECK(parseOdbDirectory(matrixJob("span_ok", "UNITS=MM\n", "", " START_NAME=top\n END_NAME=BOT\n"), ok));
        CHECK(!reportHas(ok.report, "span end"));
    }
    {   // ODB++ XML: refused, not mis-read
        std::string dir = matrixJob("odx", "UNITS=MM\n", "");
        writeFile(dir + "/matrix/matrix", "<?xml version=\"1.0\"?>\n<ODX version=\"1\"><MATRIX/></ODX>\n");
        ConvertResult r = convert(dir);
        CHECK(!r.success);
        CHECK(r.error.find("bad-matrix: matrix/matrix is ODB++ XML (ODX)") == 0);
        CHECK(isOdxXml(dir + "/matrix/matrix"));
        CHECK(!isOdxXml(dir + "/steps/pcb/profile"));
    }
}

// Beyond the spec's list: ODB++ stores any file compressed with UNIX compress as <name>.z.
// demo-design ships eda/data, both components files and seven layers' features that way.
TEST(compressed_files_are_read) {
    // steps/rev_a/chk/assembly/actions/1/res/sres/sig9/meas.z of demo-design, as shipped
    static const unsigned char z[] = {0x1f, 0x9d, 0x90, 0x30, 0x40, 0xc4, 0xc8, 0x21, 0x10, 0x84, 0x13, 0x10, 0x50, 0x40, 0xcc, 0x89, 0x21, 0xa3, 0x86, 0x82, 0x29, 0x20, 0xa6, 0x1c, 0x11, 0xe8, 0x22, 0x06, 0x8e, 0x1b, 0x35, 0x40, 0xdc, 0x70, 0x01, 0x03, 0x46, 0x43, 0x8a, 0x16, 0x31, 0x6a, 0xe4, 0xe8, 0xd1, 0x61, 0x0c, 0x81, 0x04, 0x4f, 0x1e, 0x4c, 0xb8, 0xb0, 0xe1, 0xc3, 0x88, 0x13, 0x61, 0xb8, 0xa8, 0xc1, 0x30, 0xa3, 0xcc, 0x18, 0x35, 0x41, 0xc8, 0xac, 0x41, 0x83, 0x06, 0x0c, 0x1c, 0x33, 0x66, 0xe8, 0xac, 0x68, 0x83, 0xe6, 0x0c, 0x1c, 0x39, 0x14, 0xc8, 0x40, 0x59, 0x70, 0xa5, 0xc2, 0x9a, 0x2f, 0x25, 0x82, 0x90, 0xe1, 0xe2, 0x46, 0x4e, 0x99, 0x30, 0x6c, 0x7c, 0xa4, 0x6a, 0xf5, 0xe3, 0x4d, 0x19, 0x2e, 0x01};
    const std::string want = "0 19 1 N P s125\nS SG 1.1875 7.0025 1.1875 7.0025\n1 19 1 N P s125\nS SG 0.5125 0.1125 0.5440833 0.1651389\n2 19 1 N P s125\nS SG 2.7125 0.0625 2.7125 0.1225\n";
    std::string got;
    CHECK(unixDecompress(std::string((const char*)z, sizeof(z)), got));
    CHECK_EQ(got, want);
    CHECK(!unixDecompress("UNITS=MM\n", got));                      // not a compress stream
    CHECK(!unixDecompress(std::string("\x1f\x9d\x90", 3) + std::string("\xff\xff\xff", 3), got));  // a first code that is no literal

    // a parser reads <path>.z when <path> is not there; the plain file wins when both are
    std::string dir = tempDir("compressed");
    writeFile(dir + "/other", "");   // makes the folder
    {
        std::ofstream f(dir + "/features.z", std::ios::binary);
        f.write((const char*)z, sizeof(z));
    }
    CHECK(odbFileExists(dir + "/features"));
    CHECK(!odbFileExists(dir + "/nothing"));
    auto in = openOdbFile(dir + "/features");
    CHECK(in != nullptr);
    if (in) {
        std::string first;
        std::getline(*in, first);
        CHECK_EQ(first, want.substr(0, want.find('\n')));
    }
    writeFile(dir + "/features", "UNITS=MM\n$0 r100\nP 1 2 0 P 0 0\n");
    OdbFeatures plain;
    CHECK(parseFeatures(dir + "/features", plain));
    CHECK_EQ(plain.pads.size(), (size_t)1);

    // the fixture itself: its bottom copper, components and eda/data are compressed
    std::string here = __FILE__;
    std::string demo = here.substr(0, here.find_last_of('/')) + "/../../../samples/demo-design";
    OdbFeatures sigb;
    if (!odbFileExists(demo + "/steps/rev_a/layers/sigb/features")) { std::cerr << "    (samples/demo-design not found; fixture checks skipped)\n"; return; }
    CHECK(parseFeatures(demo + "/steps/rev_a/layers/sigb/features", sigb));
    CHECK(sigb.pads.size() > 5000);
    OdbComponents top;
    CHECK(parseComponents(demo + "/steps/rev_a/layers/comp_+_top/components", top));
    CHECK(top.components.size() > 100);
    OdbEdaData eda;
    CHECK(parseEdaData(demo + "/steps/rev_a/eda/data", eda));
    CHECK(eda.nets.size() > 100);
    CHECK(!eda.packages.empty());
}

// keyhole(): the grid search splices every hole where trying every pair does, point for point,
// ties included (equal distances on a snapped grid, repeated points, holes on the outline)
TEST(keyhole_fast_matches_reference) {
    std::mt19937 rng(2029);
    auto uni = [&](double a, double b) { return std::uniform_real_distribution<double>(a, b)(rng); };
    auto pick = [&](int a, int b) { return std::uniform_int_distribution<int>(a, b)(rng); };
    auto circle = [](double cx, double cy, double r, int n, bool ccw) {
        KeyholeRing c;
        for (int k = 0; k < n; k++) {
            double a = (ccw ? 1 : -1) * 2 * M_PI * k / n;
            c.push_back({cx + r * std::cos(a), cy + r * std::sin(a)});
        }
        return c;
    };
    int fastCases = 0;
    for (int trial = 0; trial < 60; trial++) {
        const int kind = trial % 5;
        KeyholeRing outer;
        std::vector<KeyholeRing> holes;
        const double W = kind == 3 ? 400 : 100, H = kind == 3 ? 2 : 80;
        const int side = pick(50, 200);
        // the board outline, either winding, walked with many points
        for (int k = 0; k < side; k++) outer.push_back({W * k / side, 0});
        for (int k = 0; k < side; k++) outer.push_back({W, H * k / side});
        for (int k = 0; k < side; k++) outer.push_back({W - W * k / side, H});
        for (int k = 0; k < side; k++) outer.push_back({0, H - H * k / side});
        if (trial % 2) std::reverse(outer.begin(), outer.end());
        if (kind == 4) outer = {{0, 0}, {W, 0}, {W, 0}, {0, 0}};  // no area: the exact winding path
        const int n = pick(100, 400);
        for (int h = 0; h < n; h++) {
            if (kind == 0) {  // via clearances on a 1 mm grid: equal distances everywhere
                holes.push_back(circle(pick(2, (int)W - 2), pick(2, (int)H - 2), 0.3, pick(4, 16), pick(0, 1)));
            } else if (kind == 1) {  // integer points in a small box: repeats and ties
                KeyholeRing r;
                int m = pick(1, 7);
                for (int k = 0; k < m; k++) r.push_back({(double)pick(0, 12), (double)pick(0, 12)});
                holes.push_back(r);
            } else if (kind == 2) {  // anywhere, some on the outline's own points
                if (pick(0, 9) == 0) {
                    auto& p = outer[pick(0, (int)outer.size() - 1)];
                    holes.push_back({p, {p.first + 0.5, p.second}, {p.first, p.second + 0.5}});
                } else {
                    holes.push_back(circle(uni(1, W - 1), uni(1, H - 1), uni(0.05, 0.8), pick(3, 24), pick(0, 1)));
                }
            } else {  // the thin box and the flat outline: random small holes, a repeat now and then
                if (!holes.empty() && pick(0, 7) == 0) holes.push_back(holes[pick(0, (int)holes.size() - 1)]);
                else holes.push_back(circle(uni(0, W), uni(0, H), uni(0.01, 0.5), pick(3, 12), pick(0, 1)));
            }
        }
        double pairs = 0, size = (double)outer.size();
        for (auto& h : holes)
            if (h.size() >= 3) { pairs += size * h.size(); size += h.size() + 2; }
        if (pairs >= 250000) fastCases++;
        auto want = keyholeReference(outer, holes);
        auto got = keyhole(outer, holes);
        CHECK_EQ(got.size(), want.size());
        bool same = got == want;
        if (!same) std::cerr << "    trial " << trial << " (kind " << kind << ") differs\n";
        CHECK(same);
    }
    CHECK(fastCases >= 50);  // the cases do reach the grid search
    // tiny inputs, and an empty outline, go through unchanged
    CHECK(keyhole({}, {{{0, 0}, {1, 0}, {0, 1}}}).empty());
    KeyholeRing tri = {{0, 0}, {10, 0}, {0, 10}};
    CHECK(keyhole(tri, {{{1, 1}, {2, 1}}}) == tri);
    CHECK(keyhole(tri, {{{1, 1}, {2, 1}, {1, 2}}}) == keyholeReference(tri, {{{1, 1}, {2, 1}, {1, 2}}}));
}

int main(int argc, char** argv) {
    std::string only = argc > 1 ? argv[1] : "";
    int run = 0, failedTests = 0;
    for (auto& [name, fn] : registry()) {
        if (!only.empty() && name.find(only) == std::string::npos) continue;
        int before = gFailures;
        fn();
        run++;
        bool ok = gFailures == before;
        if (!ok) failedTests++;
        std::cout << (ok ? "PASS " : "FAIL ") << name << "\n";
    }
    std::cout << run - failedTests << "/" << run << " unit tests passed\n";
    return failedTests == 0 ? 0 : 1;
}
