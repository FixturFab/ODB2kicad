// odb2kicad unit tests: one or more cases per defect found by round-tripping KiCad boards
// through kicad-cli's ODB++ export.
// Designs are built in memory (writer cases) or written as tiny ODB++ files (parser cases).
//
//   ./test/run_unit_tests.sh            (g++ build + run, no cmake needed)
//   cmake --build build && ctest --test-dir build
#include "model/odb_design.h"
#include "model/kicad_pcb.h"
#include "odb_parser/odb_parser.h"
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
