#include "writer/kicad_writer.h"
#include "util/coord.h"
#include "util/string_utils.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <map>
#include <sstream>

namespace odb2kicad {

// Layer name mapping: ODB++ uppercase -> KiCad canonical
struct LayerMapping {
    int id;
    const char* canonical;
    const char* alias;      // nullptr if no alias
};

static const LayerMapping kLayerMappings[] = {
    { 0,  "F.Cu",       nullptr },
    { 31, "B.Cu",       nullptr },
    { 32, "B.Adhes",    "B.Adhesive" },
    { 33, "F.Adhes",    "F.Adhesive" },
    { 34, "B.Paste",    nullptr },
    { 35, "F.Paste",    nullptr },
    { 36, "B.SilkS",    "B.Silkscreen" },
    { 37, "F.SilkS",    "F.Silkscreen" },
    { 38, "B.Mask",     "B.Mask" },
    { 39, "F.Mask",     "F.Mask" },
    { 40, "Dwgs.User",  "User.Drawings" },
    { 41, "Cmts.User",  "User.Comments" },
    { 42, "Eco1.User",  "User.Eco1" },
    { 43, "Eco2.User",  "User.Eco2" },
    { 44, "Edge.Cuts",  nullptr },
    { 45, "Margin",     nullptr },
    { 46, "B.CrtYd",    "B.Courtyard" },
    { 47, "F.CrtYd",    "F.Courtyard" },
    { 48, "B.Fab",      nullptr },
    { 49, "F.Fab",      nullptr },
};

static const LayerMapping* findLayerByOdbName(const std::string& odbName) {
    std::string upper = toUpper(odbName);
    for (auto& m : kLayerMappings) {
        if (toUpper(m.canonical) == upper) return &m;
        if (m.alias && toUpper(m.alias) == upper) return &m;
    }
    return nullptr;
}

static std::string formatFloat(double v, int precision = 6) {
    // Remove trailing zeros
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(precision) << v;
    std::string s = oss.str();
    if (s.find('.') != std::string::npos) {
        auto last = s.find_last_not_of('0');
        if (s[last] == '.') last--; // keep at least one digit before removing dot? no, remove dot too
        // Actually: "100.000000" -> "100", "1.500000" -> "1.5"
        if (s[last] == '.') {
            s = s.substr(0, last);
        } else {
            s = s.substr(0, last + 1);
        }
    }
    // Handle -0
    if (s == "-0") s = "0";
    return s;
}

static std::string makeUuid(int index) {
    // Deterministic UUID from index
    char buf[64];
    snprintf(buf, sizeof(buf), "%08x-%04x-%04x-%04x-%012x",
             index + 1, index + 1, index + 1, index + 1, index + 1);
    return buf;
}

KicadPcb transformToKicad(const OdbDesign& design) {
    KicadPcb pcb;

    // 1. Map layers
    for (auto& odbLayer : design.layers) {
        auto* mapping = findLayerByOdbName(odbLayer.name);
        if (!mapping) continue;
        // Skip dielectric, component meta-layers
        if (odbLayer.type == "DIELECTRIC" || odbLayer.type == "COMPONENT") continue;

        KicadLayer kl;
        kl.id = mapping->id;
        kl.canonicalName = mapping->canonical;
        kl.type = (mapping->id <= 31) ? "signal" : "user";
        if (mapping->alias) kl.alias = mapping->alias;
        pcb.layers.push_back(kl);
    }

    // Sort layers by ID
    std::sort(pcb.layers.begin(), pcb.layers.end(),
              [](const KicadLayer& a, const KicadLayer& b) { return a.id < b.id; });

    // Remove duplicate layers (same ID)
    pcb.layers.erase(
        std::unique(pcb.layers.begin(), pcb.layers.end(),
                    [](const KicadLayer& a, const KicadLayer& b) { return a.id == b.id; }),
        pcb.layers.end());

    // 2. Build net list from EDA data
    // Net 0 is always the unconnected net
    {
        KicadNet n0;
        n0.id = 0;
        n0.name = "";
        pcb.nets.push_back(n0);
    }

    // Build name->kicad_id map
    std::map<std::string, int> netNameToId;
    netNameToId[""] = 0;
    netNameToId["$NONE$"] = 0;

    int nextNetId = 1;
    for (auto& edaNet : design.edaData.nets) {
        if (edaNet.name == "$NONE$") continue;
        KicadNet kn;
        kn.id = nextNetId;
        kn.name = edaNet.name;
        pcb.nets.push_back(kn);
        netNameToId[edaNet.name] = nextNetId;
        nextNetId++;
    }

    // Also add nets from netlist that aren't in EDA data
    for (auto& nd : design.netlist.nets) {
        if (netNameToId.find(nd.name) == netNameToId.end()) {
            KicadNet kn;
            kn.id = nextNetId;
            kn.name = nd.name;
            pcb.nets.push_back(kn);
            netNameToId[nd.name] = nextNetId;
            nextNetId++;
        }
    }

    // Build EDA net index -> name mapping
    // In eda/data, nets are 0-indexed: index 0 = $NONE$, 1 = first real net, etc.
    std::map<int, std::string> edaNetIdxToName;
    for (int i = 0; i < (int)design.edaData.nets.size(); i++) {
        edaNetIdxToName[i] = design.edaData.nets[i].name;
    }

    // Helper to resolve ODB net index (from TOP record) to KiCad net ID
    auto resolveNet = [&](int odbNetIdx) -> std::pair<int, std::string> {
        auto it = edaNetIdxToName.find(odbNetIdx);
        if (it != edaNetIdxToName.end()) {
            auto it2 = netNameToId.find(it->second);
            if (it2 != netNameToId.end()) {
                return {it2->second, it->second};
            }
        }
        return {0, ""};
    };

    // Helper: ODB layer name -> KiCad canonical name
    auto mapLayerName = [](const std::string& odbName) -> std::string {
        auto* m = findLayerByOdbName(odbName);
        return m ? m->canonical : odbName;
    };

    // 3. Transform components -> footprints
    auto processComponents = [&](const OdbComponents& comps, bool isBottom) {
        for (int ci = 0; ci < (int)comps.components.size(); ci++) {
            auto& comp = comps.components[ci];
            KicadFootprint fp;

            // Library name
            if (comp.footprint.find(':') != std::string::npos) {
                fp.library = comp.footprint;
            } else {
                fp.library = "imported:" + comp.footprint;
            }

            fp.layer = isBottom ? "B.Cu" : "F.Cu";
            fp.uuid = makeUuid(isBottom ? ci + 1000 : ci);
            fp.x = toKicadX(comp.x);
            fp.y = toKicadY(comp.y);
            fp.angle = comp.rotation;  // Use as-is for top
            fp.refdes = comp.refdes;
            fp.value = comp.properties.count("Value") ? comp.properties.at("Value") : "";

            if (comp.mountType == 1) fp.attr = "smd";
            else if (comp.mountType == 2) fp.attr = "through_hole";
            else fp.attr = "smd"; // default

            // Process terminals -> pads
            for (auto& term : comp.terminals) {
                KicadPad pad;
                pad.pinNum = term.pinNum;

                // Determine pad shape from the feature layer
                // Find the symbol used for this pad
                std::string cuLayerName = isBottom ? "b.cu" : "f.cu";
                auto featIt = design.layerFeatures.find(cuLayerName);
                OdbSymbol padSym;
                padSym.shape = OdbSymbol::ROUNDRECT; // default

                if (featIt != design.layerFeatures.end()) {
                    // Find the pad feature at matching coordinates
                    for (auto& p : featIt->second.pads) {
                        if (std::abs(p.x - term.x) < 0.01 && std::abs(p.y - term.y) < 0.01) {
                            if (p.symIdx < (int)featIt->second.symbols.size()) {
                                padSym = featIt->second.symbols[p.symIdx];
                            }
                            break;
                        }
                    }
                }

                switch (padSym.shape) {
                    case OdbSymbol::ROUND: pad.shape = "circle"; break;
                    case OdbSymbol::RECT: pad.shape = "rect"; break;
                    case OdbSymbol::ROUNDRECT: pad.shape = "roundrect"; break;
                    case OdbSymbol::OVAL: pad.shape = "oval"; break;
                    case OdbSymbol::SQUARE: pad.shape = "rect"; break;
                    default: pad.shape = "roundrect"; break;
                }

                pad.width = padSym.width;
                pad.height = padSym.height;
                if (padSym.shape == OdbSymbol::ROUNDRECT && padSym.cornerRadius > 0) {
                    double minDim = std::min(padSym.width, padSym.height);
                    pad.roundrectRatio = (minDim > 0) ? padSym.cornerRadius / minDim : 0;
                }

                // Pad position: absolute -> relative to component
                double dx = term.x - comp.x;
                double dy = term.y - comp.y;
                double angleRad = degToRad(comp.rotation);
                double relX =  dx * std::cos(angleRad) + dy * std::sin(angleRad);
                double relY = -dx * std::sin(angleRad) + dy * std::cos(angleRad);
                pad.x = relX;
                pad.y = relY;  // Y-negate happens at footprint level

                // Pad type
                pad.type = (comp.mountType == 2) ? "thru_hole" : "smd";

                // Pad layers
                if (pad.type == "thru_hole") {
                    pad.layerNames = {"*.Cu", "*.Mask"};
                } else {
                    if (isBottom) {
                        pad.layerNames = {"B.Cu", "B.Paste", "B.Mask"};
                    } else {
                        pad.layerNames = {"F.Cu", "F.Paste", "F.Mask"};
                    }
                }

                // Net assignment from terminal
                auto [netId, netName] = resolveNet(term.netIdx);
                pad.netId = netId;
                pad.netName = netName;

                fp.pads.push_back(pad);
            }

            pcb.footprints.push_back(fp);
        }
    };

    processComponents(design.topComponents, false);
    processComponents(design.botComponents, true);

    // 4. Transform copper traces
    for (auto& [layerNameLower, features] : design.layerFeatures) {
        auto* mapping = findLayerByOdbName(layerNameLower);
        if (!mapping) continue;
        // Only copper layers
        if (mapping->id != 0 && mapping->id != 31) continue;

        std::string kicadLayer = mapping->canonical;

        for (auto& line : features.lines) {
            // Skip lines that are part of pads (they're traces)
            KicadSegment seg;
            seg.x1 = toKicadX(line.x1);
            seg.y1 = toKicadY(line.y1);
            seg.x2 = toKicadX(line.x2);
            seg.y2 = toKicadY(line.y2);

            // Width from symbol
            if (line.symIdx < (int)features.symbols.size()) {
                auto& sym = features.symbols[line.symIdx];
                if (sym.shape == OdbSymbol::ROUND) {
                    seg.width = sym.diameter;
                } else {
                    seg.width = sym.width;
                }
            }

            seg.layer = kicadLayer;

            // Net assignment: match endpoints to pad positions
            seg.netId = 0;
            for (auto& fp : pcb.footprints) {
                for (auto& pad : fp.pads) {
                    double padAbsX = fp.x + pad.x * std::cos(degToRad(fp.angle)) - pad.y * std::sin(degToRad(fp.angle));
                    double padAbsY = fp.y + pad.x * std::sin(degToRad(fp.angle)) + pad.y * std::cos(degToRad(fp.angle));
                    if ((std::abs(padAbsX - seg.x1) < 0.02 && std::abs(padAbsY - seg.y1) < 0.02) ||
                        (std::abs(padAbsX - seg.x2) < 0.02 && std::abs(padAbsY - seg.y2) < 0.02)) {
                        if (pad.netId > 0) {
                            seg.netId = pad.netId;
                            break;
                        }
                    }
                }
                if (seg.netId > 0) break;
            }

            pcb.segments.push_back(seg);
        }
    }

    // 5. Board outline from profile
    if (!design.profile.surfaces.empty()) {
        for (auto& surface : design.profile.surfaces) {
            for (auto& contour : surface.contours) {
                if (contour.isHole) continue; // Skip holes for now
                auto& pts = contour.points;
                if (pts.size() < 4) continue; // Need at least 3 unique points + closing

                // Check if it's a simple rectangle
                // Skip first point (OB starting point, same as first OS)
                // Points: [OB_start, OS1=same, OS2, OS3, OS4, OS5=same_as_OS1]
                // For a rect: 6 points where first=last
                std::vector<OdbPoint> unique;
                for (size_t i = 1; i < pts.size(); i++) {
                    // Skip duplicate closing point
                    if (i == pts.size() - 1 &&
                        std::abs(pts[i].x - pts[1].x) < 0.001 &&
                        std::abs(pts[i].y - pts[1].y) < 0.001)
                        continue;
                    unique.push_back(pts[i]);
                }

                if (unique.size() == 4) {
                    // Check if rectangular (all corners at 90 degrees)
                    double minX = unique[0].x, maxX = unique[0].x;
                    double minY = unique[0].y, maxY = unique[0].y;
                    for (auto& p : unique) {
                        minX = std::min(minX, p.x);
                        maxX = std::max(maxX, p.x);
                        minY = std::min(minY, p.y);
                        maxY = std::max(maxY, p.y);
                    }
                    bool isRect = true;
                    for (auto& p : unique) {
                        bool atCorner = (std::abs(p.x - minX) < 0.001 || std::abs(p.x - maxX) < 0.001) &&
                                        (std::abs(p.y - minY) < 0.001 || std::abs(p.y - maxY) < 0.001);
                        if (!atCorner) { isRect = false; break; }
                    }

                    if (isRect) {
                        KicadGrRect rect;
                        rect.x1 = toKicadX(minX);
                        rect.y1 = toKicadY(maxY); // negate: ODB maxY (most negative) -> smallest KiCad Y
                        rect.x2 = toKicadX(maxX);
                        rect.y2 = toKicadY(minY); // negate: ODB minY (least negative) -> largest KiCad Y
                        rect.width = 0.05;
                        rect.type = "default";
                        rect.layer = "Edge.Cuts";
                        pcb.grRects.push_back(rect);
                        continue;
                    }
                }

                // Non-rectangular: emit individual line segments
                for (size_t i = 1; i < pts.size() - 1; i++) {
                    KicadGrLine gl;
                    gl.x1 = toKicadX(pts[i].x);
                    gl.y1 = toKicadY(pts[i].y);
                    gl.x2 = toKicadX(pts[i + 1].x);
                    gl.y2 = toKicadY(pts[i + 1].y);
                    gl.width = 0.05;
                    gl.type = "default";
                    gl.layer = "Edge.Cuts";
                    pcb.grLines.push_back(gl);
                }
            }
        }
    }

    return pcb;
}

// --- Writer ---

static void writeIndent(std::ostream& out, int depth) {
    for (int i = 0; i < depth; i++) out << "  ";
}

void writeKicadPcb(std::ostream& out, const KicadPcb& pcb) {
    out << "(kicad_pcb (version 20240108) (generator \"odb2kicad\") (generator_version \"1.0\")\n";

    // General
    out << "  (general\n";
    out << "    (thickness " << formatFloat(pcb.boardThickness) << ")\n";
    out << "    (legacy_teardrops no)\n";
    out << "  )\n";

    out << "  (paper \"A4\")\n";

    // Layers
    out << "  (layers\n";
    for (auto& layer : pcb.layers) {
        out << "    (" << layer.id << " \"" << layer.canonicalName << "\" " << layer.type;
        if (!layer.alias.empty()) out << " \"" << layer.alias << "\"";
        out << ")\n";
    }
    out << "  )\n";

    // Setup
    out << "  (setup\n";
    out << "    (pad_to_mask_clearance 0)\n";
    out << "    (allow_soldermask_bridges_in_footprints no)\n";
    out << "    (pcbplotparams\n";
    out << "      (layerselection 0x00010fc_ffffffff)\n";
    out << "      (plot_on_all_layers_selection 0x0000000_00000000)\n";
    out << "    )\n";
    out << "  )\n";

    // Nets
    for (auto& net : pcb.nets) {
        out << "  (net " << net.id << " \"" << net.name << "\")\n";
    }

    // Footprints
    for (auto& fp : pcb.footprints) {
        out << "\n  (footprint \"" << fp.library << "\" (layer \"" << fp.layer << "\")\n";
        out << "    (tstamp " << fp.uuid << ")\n";
        out << "    (at " << formatFloat(fp.x) << " " << formatFloat(fp.y);
        if (std::abs(fp.angle) > 0.001) out << " " << formatFloat(fp.angle);
        out << ")\n";
        out << "    (property \"Reference\" \"" << fp.refdes << "\")\n";
        out << "    (property \"Value\" \"" << fp.value << "\")\n";
        out << "    (attr " << fp.attr << ")\n";

        // fp_text reference
        out << "    (fp_text reference \"" << fp.refdes << "\" (at 0 -1.65) (layer \"F.SilkS\")\n";
        out << "      (effects (font (size 1 1) (thickness 0.15)))\n";
        out << "    )\n";

        // fp_text value
        out << "    (fp_text value \"" << fp.value << "\" (at 0 1.65) (layer \"F.Fab\")\n";
        out << "      (effects (font (size 1 1) (thickness 0.15)))\n";
        out << "    )\n";

        // fp_lines
        for (auto& ln : fp.fpLines) {
            out << "    (fp_line (start " << formatFloat(ln.x1) << " " << formatFloat(ln.y1)
                << ") (end " << formatFloat(ln.x2) << " " << formatFloat(ln.y2)
                << ") (stroke (width " << formatFloat(ln.width) << ") (type solid)) (layer \""
                << ln.layer << "\"))\n";
        }

        // Pads
        for (auto& pad : fp.pads) {
            out << "    (pad \"" << pad.pinNum << "\" " << pad.type << " " << pad.shape;
            out << " (at " << formatFloat(pad.x) << " " << formatFloat(pad.y) << ")";
            out << " (size " << formatFloat(pad.width) << " " << formatFloat(pad.height) << ")";
            out << " (layers";
            for (auto& l : pad.layerNames) out << " \"" << l << "\"";
            out << ")";
            if (pad.shape == "roundrect" && pad.roundrectRatio > 0) {
                out << " (roundrect_rratio " << formatFloat(pad.roundrectRatio) << ")";
            }
            if (pad.drill > 0) {
                out << " (drill " << formatFloat(pad.drill) << ")";
            }
            if (pad.netId > 0) {
                out << " (net " << pad.netId << " \"" << pad.netName << "\")";
            }
            out << ")\n";
        }

        out << "  )\n";
    }

    // Segments
    for (auto& seg : pcb.segments) {
        out << "\n  (segment (start " << formatFloat(seg.x1) << " " << formatFloat(seg.y1)
            << ") (end " << formatFloat(seg.x2) << " " << formatFloat(seg.y2)
            << ") (width " << formatFloat(seg.width)
            << ") (layer \"" << seg.layer
            << "\") (net " << seg.netId << "))\n";
    }

    // Vias
    for (auto& via : pcb.vias) {
        out << "\n  (via (at " << formatFloat(via.x) << " " << formatFloat(via.y)
            << ") (size " << formatFloat(via.size)
            << ") (drill " << formatFloat(via.drill)
            << ") (layers \"" << via.fromLayer << "\" \"" << via.toLayer
            << "\") (net " << via.netId << "))\n";
    }

    // Graphic lines
    for (auto& gl : pcb.grLines) {
        out << "\n  (gr_line (start " << formatFloat(gl.x1) << " " << formatFloat(gl.y1)
            << ") (end " << formatFloat(gl.x2) << " " << formatFloat(gl.y2)
            << ") (stroke (width " << formatFloat(gl.width)
            << ") (type " << gl.type << ")) (fill none) (layer \"" << gl.layer << "\"))\n";
    }

    // Graphic rects
    for (auto& gr : pcb.grRects) {
        out << "\n  (gr_rect (start " << formatFloat(gr.x1) << " " << formatFloat(gr.y1)
            << ") (end " << formatFloat(gr.x2) << " " << formatFloat(gr.y2)
            << ") (stroke (width " << formatFloat(gr.width)
            << ") (type " << gr.type << ")) (fill none) (layer \"" << gr.layer << "\"))\n";
    }

    // Zones
    for (auto& zone : pcb.zones) {
        out << "\n  (zone (net " << zone.netId << ") (net_name \"" << zone.netName
            << "\") (layer \"" << zone.layer << "\")\n";
        out << "    (fill (thermal_gap 0.5) (thermal_bridge_width 0.5))\n";
        if (!zone.outlinePoints.empty()) {
            out << "    (polygon\n      (pts\n";
            for (auto& [x, y] : zone.outlinePoints) {
                out << "        (xy " << formatFloat(x) << " " << formatFloat(y) << ")\n";
            }
            out << "      )\n    )\n";
        }
        for (auto& poly : zone.filledPolygons) {
            out << "    (filled_polygon\n      (layer \"" << zone.layer << "\")\n      (pts\n";
            for (auto& [x, y] : poly) {
                out << "        (xy " << formatFloat(x) << " " << formatFloat(y) << ")\n";
            }
            out << "      )\n    )\n";
        }
        out << "  )\n";
    }

    out << ")\n";
}

} // namespace odb2kicad
