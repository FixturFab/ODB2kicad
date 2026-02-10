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

struct KicadPad {
    std::string pinNum;
    std::string type;           // "smd" or "thru_hole"
    std::string shape;          // "roundrect", "circle", "rect", "oval"
    double x = 0, y = 0;       // relative to footprint
    double width = 0, height = 0;
    std::vector<std::string> layerNames;
    double roundrectRatio = 0;
    int netId = 0;
    std::string netName;
    double drill = 0;           // for thru_hole
};

struct KicadFpLine {
    double x1 = 0, y1 = 0, x2 = 0, y2 = 0;
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
    std::vector<KicadPad> pads;
    std::vector<KicadFpLine> fpLines;
};

struct KicadSegment {
    double x1 = 0, y1 = 0, x2 = 0, y2 = 0;
    double width = 0;
    std::string layer;
    int netId = 0;
};

struct KicadVia {
    double x = 0, y = 0;
    double size = 0;
    double drill = 0;
    std::string fromLayer;
    std::string toLayer;
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

struct KicadZone {
    int netId = 0;
    std::string netName;
    std::string layer;
    std::vector<std::pair<double,double>> outlinePoints;
    std::vector<std::vector<std::pair<double,double>>> filledPolygons;
};

struct KicadPcb {
    double boardThickness = 1.6;
    std::vector<KicadLayer> layers;
    std::vector<KicadNet> nets;
    std::vector<KicadFootprint> footprints;
    std::vector<KicadSegment> segments;
    std::vector<KicadVia> vias;
    std::vector<KicadGrLine> grLines;
    std::vector<KicadGrRect> grRects;
    std::vector<KicadGrArc> grArcs;
    std::vector<KicadZone> zones;
};

} // namespace odb2kicad
