#pragma once
#include <string>
#include <vector>
#include <map>

namespace odb2kicad {

struct OdbInfo {
    std::string units = "MM";
    std::string jobName;
    std::string odbSource;
};

struct OdbLayer {
    int row = 0;
    std::string context;    // BOARD, MISC
    std::string type;       // SIGNAL, COMPONENT, SILK_SCREEN, SOLDER_MASK, SOLDER_PASTE, DIELECTRIC, DRILL, DOCUMENT
    std::string name;       // e.g. F.CU, B.SILKSCREEN
    std::string polarity;   // POSITIVE, NEGATIVE
    std::string startName;  // for drill layers
    std::string endName;    // for drill layers
    std::string dielectricType; // CORE, PREPREG
};

struct OdbStepHeader {
    double xOrigin = 0;
    double yOrigin = 0;
    double xDatum = 0;
    double yDatum = 0;
    std::string units = "MM";
};

struct OdbPoint {
    double x = 0;
    double y = 0;
};

struct OdbContour {
    bool isHole = false;    // true = H (hole), false = I (island)
    std::vector<OdbPoint> points;
};

struct OdbSurface {
    std::string polarity;   // P or N
    std::vector<OdbContour> contours;
};

struct OdbProfile {
    std::vector<OdbSurface> surfaces;
};

// Symbol definition decoded from name
struct OdbSymbol {
    std::string name;       // raw name e.g. "rect1025.0x1400.0xr250.0"
    enum Shape { ROUND, RECT, ROUNDRECT, OVAL, SQUARE, UNKNOWN } shape = UNKNOWN;
    double width = 0;       // mm
    double height = 0;      // mm
    double diameter = 0;    // mm (for round)
    double cornerRadius = 0;// mm (for roundrect)
};

// Feature records from layer features files
struct OdbPad {
    double x = 0, y = 0;
    int symIdx = 0;
    std::string polarity;   // P or N
    int mirror = 0;
    double angle = 0;
    std::map<int, std::string> attrs; // attr_idx -> value
};

struct OdbLine {
    double x1 = 0, y1 = 0, x2 = 0, y2 = 0;
    int symIdx = 0;
    std::string polarity;
    std::map<int, std::string> attrs;
};

struct OdbArc {
    double xs = 0, ys = 0;  // start
    double xe = 0, ye = 0;  // end
    double xc = 0, yc = 0;  // center
    int symIdx = 0;
    std::string polarity;
    bool clockwise = false;
    std::map<int, std::string> attrs;
};

struct OdbFeatures {
    std::string units;
    std::vector<OdbSymbol> symbols;
    std::map<int, std::string> attrDefs;  // @idx -> name
    std::map<int, std::string> strings;   // &idx -> text
    std::vector<OdbPad> pads;
    std::vector<OdbLine> lines;
    std::vector<OdbArc> arcs;
    std::vector<OdbSurface> surfaces;
};

// Component placement
struct OdbTerminal {
    int pinIdx = 0;
    double x = 0, y = 0;
    double rotation = 0;
    std::string mirror;     // N or M
    int netIdx = 0;
    int subnetIdx = 0;
    std::string pinNum;
};

struct OdbComponent {
    int pkgIdx = 0;
    double x = 0, y = 0;
    double rotation = 0;
    std::string mirror;     // N or M
    std::string refdes;
    std::string footprint;
    std::map<std::string, std::string> properties; // key -> value
    std::vector<OdbTerminal> terminals;
    int mountType = 0;      // 1=SMD, 2=TH
};

struct OdbComponents {
    std::string units;
    std::map<int, std::string> attrDefs;
    std::map<int, std::string> strings;
    std::vector<OdbComponent> components;
};

// EDA data
struct OdbEdaNet {
    std::string name;
};

struct OdbEdaPin {
    std::string id;
    std::string side;       // S (SMD)
    double x = 0, y = 0;   // relative to package center
    double rotation = 0;
    std::string electrical; // E
    std::string shape;      // S
};

struct OdbEdaPackage {
    std::string name;
    std::vector<OdbEdaPin> pins;
};

struct OdbEdaData {
    std::string units;
    std::vector<std::string> layers;
    std::vector<OdbEdaNet> nets;        // index 0 = $NONE$
    std::vector<OdbEdaPackage> packages;
};

// Netlist
struct OdbNetDef {
    int index = 0;
    std::string name;
};

struct OdbNetPoint {
    int netIdx = 0;
    double viaFlag = 0;
    double x = 0, y = 0;
    std::string layer;      // T, B
    double padW = 0, padH = 0;
    bool isVia = false;
};

struct OdbNetlist {
    std::vector<OdbNetDef> nets;
    std::vector<OdbNetPoint> points;
};

// Top-level design
struct OdbDesign {
    OdbInfo info;
    std::vector<OdbLayer> layers;
    std::string stepName;
    OdbStepHeader stepHeader;
    OdbProfile profile;
    std::map<std::string, OdbFeatures> layerFeatures; // layer name (lowercase) -> features
    OdbComponents topComponents;
    OdbComponents botComponents;
    OdbEdaData edaData;
    OdbNetlist netlist;
};

} // namespace odb2kicad
