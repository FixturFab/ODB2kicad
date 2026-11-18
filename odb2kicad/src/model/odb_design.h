#pragma once
#include <string>
#include <vector>
#include <map>

namespace odb2kicad {

struct OdbInfo {
    std::string units;      // misc/info UNITS (empty: none); the default for files without UNITS
    std::string jobName;    // JOB_NAME, else the one line of misc/job_name
    std::string odbSource;
    std::string saveApp;    // SAVE_APP, e.g. "KiCad EDA 9.0.9"
    int versionMajor = -1;  // ODB_VERSION_MAJOR / ODB_VERSION_MINOR (-1: the job does not say)
    int versionMinor = -1;
    std::string creationDate;   // CREATION_DATE, as written (yyyymmdd.hhmmss)
    std::string saveDate;       // SAVE_DATE
    std::map<std::string, std::string> values;  // every key of misc/info, as written
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
    std::string oldName;    // OLD_NAME: the name before a rename (NAME is the layer)
    std::string color;      // COLOR, as written (informational)
    std::string id;         // ID, as written
    std::string writtenType;// the TYPE the matrix wrote, when it is not a standard one (type is then DOCUMENT)
};

// A STEP record of the matrix
struct OdbMatrixStep {
    int col = 0;
    std::string name;
};

// A STEP-REPEAT block of a stephdr: the step NAME placed NX x NY times in this one (a panel).
// Instance (i, j) puts the named step's datum at (X + i*DX, Y + j*DY); about that point the step
// is mirrored in X (MIRROR) and then turned ANGLE degrees clockwise. FLIP: the step's bottom
// side is up. Lengths in the stephdr's units until normalizeUnits makes them mm.
struct OdbStepRepeat {
    std::string name;
    double x = 0, y = 0;
    double dx = 0, dy = 0;
    int nx = 1, ny = 1;
    double angle = 0;
    bool mirror = false;
    bool flip = false;
};

struct OdbStepHeader {
    // X_ORIGIN / Y_ORIGIN: where the step's origin sits. Every coordinate of the step is
    // relative to it; normalizeUnits adds it (x' = x + X_ORIGIN) to the profile, the layer
    // features, the component placements and the netlist points. The datum is a reference
    // point for alignment, not a transform.
    double xOrigin = 0;
    double yOrigin = 0;
    double xDatum = 0;
    double yDatum = 0;
    std::string units;      // the stephdr's UNITS line (empty: none)
    int stepRepeats = 0;                // STEP-REPEAT blocks: other steps placed in this one (a panel)
    std::vector<OdbStepRepeat> repeats; // those blocks, in file order
    bool originApplied = false;         // applyStepOrigin has moved the step
    double appliedX = 0, appliedY = 0;  // by this much (mm)
};

struct OdbPoint {
    double x = 0;
    double y = 0;
    // An OC record ends at this point: the edge from the previous vertex is an arc about
    // (xc, yc). The arc is also flattened into `interp` points before it, so polygon users
    // can ignore these fields; outline writers skip the interp points and draw the arc.
    bool arc = false;
    bool cw = false;
    double xc = 0, yc = 0;
    bool interp = false;
};

struct OdbContour {
    bool isHole = false;    // true = H (hole), false = I (island)
    std::vector<OdbPoint> points;
};

struct OdbSurface {
    int featIdx = -1;       // ordinal of the S record among the layer's features (eda/data FID)
    std::string polarity;   // P or N
    std::vector<OdbContour> contours;
};

struct OdbProfile {
    std::string units;      // the profile file's UNITS line (empty: none)
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
    double lineWidth = 0;   // mm (donut ring width: donut_rc line width, donut_r (od-id)/2)
    bool donut = false;     // donut_r / donut_rc: an outline, not a filled shape
    char unitOverride = 0;  // 'I' / 'M' after the name ("$3 rect50x30 I"): mils / microns whatever the file's UNITS
    // Every other standard symbol (shape stays UNKNOWN): its outline as rings filled even-odd,
    // symbol frame (y up, unrotated), same units as width: thermals, chamfered and
    // part-rounded rects, octagons, ellipses, diamonds, hexagons, triangles, butterflies, donuts.
    std::vector<std::vector<std::pair<double,double>>> outline;
    // Further shapes (each filled even-odd, same frame) unioned with `outline`, and shapes cut out
    // of that union: thermal air gaps and open corners, moire crosshairs, stencil-symbol fillets.
    std::vector<std::vector<std::vector<std::pair<double,double>>>> outlineAdd, outlineCut;
};

// Feature records from layer features files
struct OdbPad {
    int featIdx = -1;       // ordinal among the layer's features (eda/data FID)
    double x = 0, y = 0;
    int symIdx = 0;
    std::string polarity;   // P or N
    int dcode = 0;          // the dcode field; on a drill layer, the NUM of the hit's tool (0: none)
    int mirror = 0;
    double angle = 0;
    std::map<int, std::string> attrs; // attr_idx -> value
};

struct OdbLine {
    int featIdx = -1;
    int textIdx = -1;       // >= 0: a stroke of OdbFeatures::texts[textIdx], drawn from its font
    double x1 = 0, y1 = 0, x2 = 0, y2 = 0;
    int symIdx = 0;
    std::string polarity;
    std::map<int, std::string> attrs;
};

struct OdbArc {
    int featIdx = -1;
    double xs = 0, ys = 0;  // start
    double xe = 0, ye = 0;  // end
    double xc = 0, yc = 0;  // center
    int symIdx = 0;
    std::string polarity;
    bool clockwise = false;
    bool directionGiven = false;  // the record carries its Y|N token (false: read counter-clockwise)
    std::map<int, std::string> attrs;
};

// A T (or V) record: text drawn with a vector font (fonts/<font>)
struct OdbText {
    int featIdx = -1;
    double x = 0, y = 0;    // where the first glyph's zero goes
    std::string font;       // font name, e.g. "standard"
    std::string polarity;   // P or N
    bool mirror = false;    // orient 4-7 / 9: mirrored in X before it is turned
    double angle = 0;       // degrees, clockwise
    double xsize = 0, ysize = 0;  // character cell size
    double weight = 1;      // stroke width, in font line widths (12 mil each in the standard font)
    std::string text;       // the string, without its quotes; $$JOB / $$STEP / $$LAYER filled in
    int version = 1;        // the trailing field: 1 current, 0 previous
    bool vertical = false;  // a V record: the characters stacked along +y
    std::map<int, std::string> attrs;
    int strokes = 0;        // strokes drawn for it (0: no font, or nothing but blanks)
};

struct OdbFontStroke {
    double x1 = 0, y1 = 0, x2 = 0, y2 = 0;
    bool positive = true;
    bool round = true;      // R round ends, S square
    double width = 0;
};

// fonts/<name>: values as written (inches unless the file says UNITS=MM)
struct OdbFont {
    std::string units;
    double xsize = 0, ysize = 0;    // the glyph box; a character advances one XSIZE
    double offset = 0;              // OFFSET: the baseline's y offset (kept, not applied)
    std::map<char, std::vector<OdbFontStroke>> glyphs;   // every CHAR, its LINE strokes
    std::map<char, std::vector<std::vector<std::pair<double, double>>>> polygons;  // its POLYGON outlines
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
    std::vector<OdbText> texts;           // T / V records; their strokes are in `lines` (textIdx)
    int barcodes = 0;                     // B records (not converted)
};

// One tool of a drill layer's tools file
struct OdbDrillTool {
    int num = -1;
    std::string type;       // VIA, PLATED, NON_PLATED, COMPANY, CCE (upper case)
    std::string type2;      // STANDARD, SMOOTH, ROUGH (older jobs: PHOTO, LASER, PRESS_FIT)
    std::string bit;        // drill bit name as written, e.g. "+77", "1.3MM" (signed: taper / backdrill side)
    double minTol = 0, maxTol = 0;
    double finishSize = 0;  // finished hole
    double drillSize = 0;   // the tool: what the hole is drilled with
};

// layers/<drill>/tools. THICKNESS, the tolerances and the sizes are written in thousandths of
// the file's unit (mils for INCH, microns for MM); normalizeUnits makes them mm.
struct OdbDrillTools {
    bool present = false;   // the file exists (it may hold no tools)
    std::string units;      // its UNITS line (empty: none)
    double thickness = 0;   // board thickness the layer was drilled through
    std::string userParams;
    std::vector<OdbDrillTool> tools;
};

// An attrlist file: "key = value" lines (job misc/attrlist, a step's, a layer's)
struct OdbAttrList {
    bool present = false;   // the file exists (KiCad writes empty placeholders)
    std::string units;      // its UNITS line (empty: none)
    std::map<std::string, std::string> values;  // every other key, as written

    bool has(const std::string& key) const;     // present and not empty
    std::string text(const std::string& key) const;
    double number(const std::string& key, double fallback = 0) const;
    bool flag(const std::string& key) const;    // yes / y / true / 1
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
    bool noPop = false;     // .no_pop attribute (KiCad writes it for DNP footprints)
};

struct OdbComponents {
    std::string units;
    std::map<int, std::string> attrDefs;
    std::map<int, std::string> strings;
    std::vector<OdbComponent> components;
};

// A COMPONENT matrix row other than comp_+_top / comp_+_bot (rigid-flex: comp_+_left, ...)
struct OdbComponentLayer {
    std::string name;       // layer name, lowercase
    bool bottom = false;    // mounted on the bottom side (the row lies below the first copper row)
    OdbComponents components;
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
    // Pin outline: the RC / CR / SQ / CT records after the PIN, as closed rings (islands only,
    // arcs flattened) in the package frame, like OdbEdaPackage::outlines. The pin's land as the
    // CAD system holds it; the pad shape when the pin has no copper flash to take it from.
    std::vector<std::vector<std::pair<double,double>>> outlines;
};

struct OdbEdaPackage {
    std::string name;
    std::vector<OdbEdaPin> pins;
    // Package outline: the RC/CR/SQ/CT records between PKG and its first PIN, as closed rings
    // (islands only, arcs flattened) in the package frame, eda/data units. KiCad writes the
    // footprint courtyard here (its bounding hull when it has none), Allegro the place-bound,
    // Altium the body.
    std::vector<std::vector<std::pair<double,double>>> outlines;
};

// FID record: feature <featIdx> of layer LYR[<layerIdx>] belongs to net <netIdx>
struct OdbEdaFid {
    int netIdx = 0;         // index into OdbEdaData::nets
    int sntIdx = -1;        // ordinal of the SNT record it belongs to (one per subnet: a zone, a pin...)
    std::string sntType;    // TOP (component pin), TRC, VIA, PLN (plane / zone)
    std::string sntSide;    // TOP only: T or B
    int sntComp = -1;       // TOP only: component index in comp_+_top / comp_+_bot
    int sntToeprint = -1;   // TOP only: toeprint (pin) index within the component
    std::string type;       // C copper, L laminate, H hole
    int layerIdx = 0;       // index into OdbEdaData::layers (the LYR record)
    int featIdx = 0;        // ordinal of the feature in that layer's features file
};

struct OdbEdaData {
    std::string units;
    std::vector<std::string> layers;
    std::vector<OdbEdaNet> nets;        // index 0 = $NONE$
    std::vector<OdbEdaPackage> packages;
    std::vector<OdbEdaFid> fids;        // feature -> net map (SNT/FID records)
};

// Netlist
struct OdbNetDef {
    int index = 0;
    std::string name;
};

// One netlist point: <net> <radius> <x> <y> <side> [<w> <h>] <epoint> <exp> [...] [v]
struct OdbNetPoint {
    int netIdx = 0;
    double radius = 0;      // drill radius; 0 for a pad without a hole
    double x = 0, y = 0;
    std::string layer;      // T top, D down (bottom), B both, or an inner layer number
    bool hasPadSize = false;// <w> <h> were written (they are for radius 0 only)
    double padW = 0, padH = 0;
    std::string epoint;     // e net end point, m mid point
    std::string exp;        // e exposed, c covered, p covered primary side, s covered secondary side
    bool isVia = false;     // the trailing "v" token
};

struct OdbNetlist {
    std::string units;
    std::vector<OdbNetDef> nets;
    std::vector<OdbNetPoint> points;
};

// Top-level design
struct OdbDesign {
    OdbInfo info;
    std::vector<OdbLayer> layers;
    std::vector<OdbMatrixStep> steps;   // every STEP record of the matrix
    std::string stepName;               // the step this design holds
    OdbStepHeader stepHeader;
    OdbProfile profile;
    std::map<std::string, OdbFeatures> layerFeatures; // layer name (lowercase) -> features
    std::map<std::string, OdbFeatures> userSymbols;   // symbols/<name>/features (name lowercase)
    std::map<std::string, OdbFont> fonts;             // fonts/<name> (name lowercase), read when a text needs one
    std::map<std::string, OdbDrillTools> layerTools;  // layer name (lowercase) -> its tools file
    std::map<std::string, OdbAttrList> layerAttrs;    // layer name (lowercase) -> its attrlist
    OdbAttrList jobAttrs;                             // misc/attrlist
    OdbAttrList stepAttrs;                            // steps/<step>/attrlist
    // Board thickness in mm (0: the job says nothing), set by computeBoardThickness, and where it
    // came from: "board_thickness" (an explicit .board_thickness), "attrlist" (the stackup rows), "tools" (a drill table's THICKNESS) or "default"
    double boardThickness = 0;
    std::string thicknessSource;
    // What the reader did that the job's author might not expect: "warning: ..." lines for what it
    // had to assume or drop, "info: ..." for choices it made. Carried into ConvertResult::report.
    std::vector<std::string> report;
    OdbComponents topComponents;
    OdbComponents botComponents;
    std::vector<OdbComponentLayer> otherComponents;   // further COMPONENT rows
    OdbEdaData edaData;
    OdbNetlist netlist;
    bool unitsNormalized = false;  // every coordinate and size is in mm (normalizeUnits)
    // DOCUMENT layers without a KiCad layer of their name are drawn on free User.N layers when
    // they hold at most this many features (-1: any size, 0: never). See kDefaultDocumentFeatureLimit.
    long long documentFeatureLimit = 100000;
};

} // namespace odb2kicad
