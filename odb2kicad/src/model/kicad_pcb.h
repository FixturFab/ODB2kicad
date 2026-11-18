#pragma once
#include <string>
#include <vector>
#include <map>

namespace odb2kicad {

struct KicadLayer {
    int id = 0;
    std::string canonicalName;  // e.g. "F.Cu"
    std::string type;           // "signal" or "user"
    std::string alias;          // optional alias e.g. "F.Silkscreen"
};

struct KicadNet {
    int id = 0;
    std::string name;
};

// One copper layer of a pad whose shape differs from layer to layer (a KiCad 9 padstack entry)
struct KicadPadLayer {
    std::string layer;          // "In1.Cu" ..., or "Inner" for every inner layer
    std::string shape;          // "circle", "rect", "roundrect", "oval"
    double width = 0, height = 0;
    double roundrectRatio = 0;
};

struct KicadPad {
    std::string pinNum;
    std::string type;           // "smd" or "thru_hole"
    std::string shape;          // "roundrect", "circle", "rect", "oval"
    double x = 0, y = 0;       // relative to footprint, in its unrotated frame
    double angle = 0;           // absolute orientation (KiCad stores pad angles board-relative)
    double width = 0, height = 0;
    std::vector<std::string> layerNames;
    double roundrectRatio = 0;
    int netId = 0;
    std::string netName;
    double drill = 0;           // for thru_hole (round hole diameter, or slot width)
    double drillHeight = 0;     // slot length when the hole is oval (0 = round)
    // Mask / paste opening = pad grown by this much (mm); written only when the flag is set
    bool hasMaskMargin = false, hasPasteMargin = false;
    double maskMargin = 0, pasteMargin = 0;
    // custom shape: filled polygons in the pad frame (relative to the pad, unrotated)
    std::vector<std::vector<std::pair<double,double>>> primitives;
    // Mask / paste openings the pad cannot make by growing (another shape, size ratio or corner):
    // mask- or paste-only pads of the opening's own shape, added beside this one
    std::vector<KicadPad> apertures;
    // Per-layer copper (KiCad 9 padstack). Empty: the pad is the same on every copper layer.
    // The outer layers keep the fields above; padstackMode "front_inner_back" has one "Inner"
    // entry, "custom" one entry per inner layer.
    std::string padstackMode;
    std::vector<KicadPadLayer> padstack;
};

struct KicadFpLine {
    double x1 = 0, y1 = 0, x2 = 0, y2 = 0;
    double width = 0;
    std::string layer;
};

struct KicadFpArc {
    double xs = 0, ys = 0, xm = 0, ym = 0, xe = 0, ye = 0;
    double width = 0;
    std::string layer;
};

struct KicadFpCircle {
    double cx = 0, cy = 0, r = 0;
    double width = 0;
    std::string layer;
};

struct KicadFootprint {
    std::string library;        // e.g. "Resistor_SMD:R_0805_2012Metric"
    std::string layer;          // "F.Cu" or "B.Cu"
    std::string uuid;
    double x = 0, y = 0;
    double angle = 0;
    std::string refdes;
    std::string value;
    std::string attr;           // "smd" or "through_hole"
    bool dnp = false;           // emitted as a trailing "dnp" in (attr ...)
    std::vector<std::vector<std::string>> netTiePadGroups;  // pads whose copper joins on purpose
    std::vector<KicadPad> pads;
    std::vector<KicadFpLine> fpLines;   // footprint-frame coordinates, like the pads
    std::vector<KicadFpArc> fpArcs;
    std::vector<KicadFpCircle> fpCircles;
};

struct KicadSegment {
    double x1 = 0, y1 = 0, x2 = 0, y2 = 0;
    double width = 0;
    std::string layer;
    int netId = 0;
};

struct KicadArc {
    double xs = 0, ys = 0;     // start
    double xm = 0, ym = 0;     // mid
    double xe = 0, ye = 0;     // end
    double width = 0;
    std::string layer;
    int netId = 0;
};

struct KicadVia {
    double x = 0, y = 0;
    double size = 0;            // the pad on the via's first layer
    // Pad diameter per copper layer of the span, in stack order (KiCad 9 padstack). Empty or all
    // equal: one size. A layer where the job has no copper ring (no flash, or a negative plane
    // whose flash is a clearance) has the drill's size.
    std::vector<std::pair<std::string, double>> layerSizes;
    double drill = 0;
    std::string fromLayer;
    std::string toLayer;
    bool blind = false;         // blind / buried: does not span F.Cu-B.Cu
    int netId = 0;
};

struct KicadGrLine {
    double x1 = 0, y1 = 0, x2 = 0, y2 = 0;
    double width = 0;
    std::string type;           // "solid", "default"
    std::string layer;
};

struct KicadGrRect {
    double x1 = 0, y1 = 0, x2 = 0, y2 = 0;
    double width = 0;
    std::string type;
    std::string layer;
};

struct KicadGrArc {
    double xs = 0, ys = 0;     // start
    double xm = 0, ym = 0;     // mid
    double xe = 0, ye = 0;     // end
    double width = 0;
    std::string layer;
};

struct KicadGrPoly {
    std::vector<std::pair<double,double>> pts;
    double width = 0;           // stroke
    bool filled = true;
    std::string layer;
};

struct KicadGrCircle {
    double cx = 0, cy = 0, r = 0;
    double width = 0;           // stroke
    bool filled = true;
    std::string layer;
};

struct KicadZone {
    int netId = 0;
    std::string netName;
    std::string layer;
    int priority = 0;           // file order on its layer: a later surface is on top
    std::vector<std::pair<double,double>> outlinePoints;
    std::vector<std::vector<std::pair<double,double>>> filledPolygons;
};

struct KicadPcb {
    // KiCad file format: 9 writes padstacks (per-layer via and pad sizes, version 20241229);
    // 8 gives every via and pad one size (version 20240108). 0 = 9 when a via or pad needs it.
    int fileFormat = 0;
    // (generator "..."): "odb2kicad/<version>", plus " odb++:v<major>.<minor>" when the job says
    std::string generator = "odb2kicad";
    double boardThickness = 1.6;
    std::vector<KicadLayer> layers;
    std::vector<KicadNet> nets;
    std::vector<KicadFootprint> footprints;
    std::vector<KicadSegment> segments;
    std::vector<KicadArc> arcs;
    std::vector<KicadVia> vias;
    std::vector<KicadGrLine> grLines;
    std::vector<KicadGrRect> grRects;
    std::vector<KicadGrArc> grArcs;
    std::vector<KicadGrPoly> grPolys;
    std::vector<KicadGrCircle> grCircles;
    std::vector<KicadZone> zones;
};

} // namespace odb2kicad
