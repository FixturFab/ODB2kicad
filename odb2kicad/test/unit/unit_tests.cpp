// odb2kicad unit tests: one or more cases per defect found by round-tripping KiCad boards
// through kicad-cli's ODB++ export.
// Designs are built in memory (writer cases) or written as tiny ODB++ files (parser cases).
//
//   ./test/run_unit_tests.sh            (g++ build + run, no cmake needed)
//   cmake --build build && ctest --test-dir build
#include "model/odb_design.h"
#include "model/kicad_pcb.h"
#include "odb_parser/eda_parser.h"
#include "odb_parser/features_parser.h"
#include "odb_parser/matrix_parser.h"
#include "odb_parser/odb_parser.h"
#include "odb_parser/symbols.h"
#include "writer/kicad_writer.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iostream>
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

// ---------------------------------------------------------------- main

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
