#include "writer/kicad_writer.h"
#include "util/coord.h"
#include "util/string_utils.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <map>
#include <set>
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

// Inner copper layers In1.Cu..In30.Cu (KiCad ids 1..30)
static const LayerMapping* innerLayerMapping(int n) {
    static LayerMapping innerLayers[30];
    static char names[30][16];
    static bool init = false;
    if (!init) {
        for (int i = 0; i < 30; i++) {
            snprintf(names[i], sizeof(names[i]), "In%d.Cu", i + 1);
            innerLayers[i] = { i + 1, names[i], nullptr };
        }
        init = true;
    }
    return (n >= 1 && n <= 30) ? &innerLayers[n - 1] : nullptr;
}

static bool isCopperType(const std::string& type) {
    std::string t = toUpper(type);
    return t == "SIGNAL" || t == "POWER_GROUND" || t == "MIXED";
}

// "IN<n>.CU" (KiCad's ODB++ name for In<n>.Cu) -> n, else 0
static int kicadInnerLayerNumber(const std::string& odbName) {
    std::string u = toUpper(odbName);
    if (u.size() < 6 || u.compare(0, 2, "IN") != 0 || u.compare(u.size() - 3, 3, ".CU") != 0) return 0;
    std::string digits = u.substr(2, u.size() - 5);
    if (digits.empty() || digits.find_first_not_of("0123456789") != std::string::npos) return 0;
    return std::stoi(digits);
}

// Build layer name mapping from ODB++ matrix layer stack.
// Handles both KiCad-generated (F.CU, IN1.CU, B.CU) and non-KiCad (SIGNAL_1, SIGNAL_10, etc.) naming.
static std::map<std::string, const LayerMapping*> buildLayerMap(const std::vector<OdbLayer>& layers) {
    std::map<std::string, const LayerMapping*> result;

    // First pass: try direct name matching (KiCad ODB++ output)
    for (auto& layer : layers) {
        auto* m = findLayerByOdbName(layer.name);
        if (m) {
            result[toLower(layer.name)] = m;
        }
    }

    // If we found F.Cu and B.Cu by name, this is a KiCad source: its inner copper rows
    // (IN<n>.CU) still need mapping, by name or else by row order between F.Cu and B.Cu.
    bool hasFCu = false, hasBCu = false;
    for (auto& [name, m] : result) {
        if (m->id == 0) hasFCu = true;
        if (m->id == 31) hasBCu = true;
    }
    if (hasFCu && hasBCu) {
        std::vector<std::pair<int, std::string>> unmapped; // row, name
        std::set<int> used;
        for (auto& layer : layers) {
            if (!isCopperType(layer.type) || result.count(toLower(layer.name))) continue;
            int n = kicadInnerLayerNumber(layer.name);
            if (n >= 1 && n <= 30 && !used.count(n)) {
                result[toLower(layer.name)] = innerLayerMapping(n);
                used.insert(n);
            } else {
                unmapped.push_back({layer.row, layer.name});
            }
        }
        std::sort(unmapped.begin(), unmapped.end());
        int next = 1;
        for (auto& [row, name] : unmapped) {
            while (used.count(next)) next++;
            if (next > 30) break;
            result[toLower(name)] = innerLayerMapping(next);
            used.insert(next);
        }
        return result;
    }

    // Second pass: TYPE-based mapping for non-KiCad ODB++ files
    // Collect copper layers (SIGNAL and POWER_GROUND) in row order
    std::vector<std::pair<std::string, int>> copperLayers; // name, row
    std::vector<std::pair<std::string, int>> maskLayers;
    std::vector<std::pair<std::string, int>> pasteLayers;
    std::vector<std::pair<std::string, int>> silkLayers;

    for (auto& layer : layers) {
        std::string type = toUpper(layer.type);
        if (isCopperType(type))
            copperLayers.push_back({layer.name, layer.row});
        else if (type == "SOLDER_MASK")
            maskLayers.push_back({layer.name, layer.row});
        else if (type == "SOLDER_PASTE")
            pasteLayers.push_back({layer.name, layer.row});
        else if (type == "SILK_SCREEN")
            silkLayers.push_back({layer.name, layer.row});
    }

    // Sort by row
    auto byRow = [](auto& a, auto& b) { return a.second < b.second; };
    std::sort(copperLayers.begin(), copperLayers.end(), byRow);
    std::sort(maskLayers.begin(), maskLayers.end(), byRow);
    std::sort(pasteLayers.begin(), pasteLayers.end(), byRow);
    std::sort(silkLayers.begin(), silkLayers.end(), byRow);

    // Map copper: first = F.Cu (0), last = B.Cu (31), inner = In1.Cu through In30.Cu
    if (!copperLayers.empty()) {
        result[toLower(copperLayers.front().first)] = &kLayerMappings[0]; // F.Cu
        if (copperLayers.size() > 1) {
            result[toLower(copperLayers.back().first)] = &kLayerMappings[1]; // B.Cu
        }
        for (size_t i = 1; i + 1 < copperLayers.size() && i <= 30; i++) {
            result[toLower(copperLayers[i].first)] = innerLayerMapping((int)i);
        }
    }

    // Map mask: first (top) = F.Mask (39), last (bottom) = B.Mask (38)
    if (!maskLayers.empty()) {
        result[toLower(maskLayers.front().first)] = &kLayerMappings[9]; // F.Mask (id 39)
        if (maskLayers.size() > 1) {
            result[toLower(maskLayers.back().first)] = &kLayerMappings[8]; // B.Mask (id 38)
        }
    }

    // Map paste: first = F.Paste (35), last = B.Paste (34)
    if (!pasteLayers.empty()) {
        result[toLower(pasteLayers.front().first)] = &kLayerMappings[5]; // F.Paste (id 35)
        if (pasteLayers.size() > 1) {
            result[toLower(pasteLayers.back().first)] = &kLayerMappings[4]; // B.Paste (id 34)
        }
    }

    // Map silk: first = F.SilkS (37), last = B.SilkS (36)
    if (!silkLayers.empty()) {
        result[toLower(silkLayers.front().first)] = &kLayerMappings[7]; // F.SilkS (id 37)
        if (silkLayers.size() > 1) {
            result[toLower(silkLayers.back().first)] = &kLayerMappings[6]; // B.SilkS (id 36)
        }
    }

    return result;
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

// Escape a string for emission inside a quoted s-expression token.
// ODB++ names can contain double quotes (e.g. `1/4"_Standoff`), backslashes
// or control characters; emitting them raw produces a file KiCad's strict
// parser rejects ("Failed to load board") even though lenient parsers such
// as KiCanvas accept it. Mirrors KiCad's own OUTPUTFORMATTER quoting.
static std::string escapeString(const std::string& in) {
    std::string out;
    out.reserve(in.size());
    for (char c : in) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"':  out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:   out += c;      break;
        }
    }
    return out;
}

static std::string makeUuid(int index) {
    // Deterministic UUID from index
    char buf[64];
    snprintf(buf, sizeof(buf), "%08x-%04x-%04x-%04x-%012x",
             index + 1, index + 1, index + 1, index + 1, index + 1);
    return buf;
}

// Compute arc midpoint from ODB++ center, start, end, clockwise flag.
// ODB++ uses center representation; KiCad uses start/mid/end.
// The midpoint is the point on the arc at the halfway angle.
static void computeArcMidpoint(double xs, double ys, double xe, double ye,
                                double xc, double yc, bool clockwise,
                                double& xm, double& ym) {
    double startAngle = std::atan2(ys - yc, xs - xc);
    double endAngle = std::atan2(ye - yc, xe - xc);
    double radius = std::sqrt((xs - xc) * (xs - xc) + (ys - yc) * (ys - yc));

    double sweep;
    if (clockwise) {
        // CW: start -> end going clockwise (decreasing angle)
        sweep = startAngle - endAngle;
        if (sweep <= 0) sweep += 2.0 * PI;
        double midAngle = startAngle - sweep / 2.0;
        xm = xc + radius * std::cos(midAngle);
        ym = yc + radius * std::sin(midAngle);
    } else {
        // CCW: start -> end going counter-clockwise (increasing angle)
        sweep = endAngle - startAngle;
        if (sweep <= 0) sweep += 2.0 * PI;
        double midAngle = startAngle + sweep / 2.0;
        xm = xc + radius * std::cos(midAngle);
        ym = yc + radius * std::sin(midAngle);
    }
}

// Helper to check if units string indicates inches
static bool isInchUnits(const std::string& units) {
    if (units.empty()) return true;  // ODB++ default is INCH when not specified
    std::string upper = toUpper(units);
    return upper == "INCH" || upper == "IN" || upper == "INCHES";
}

// A hit on a DRILL layer
struct DrillHit {
    double x = 0, y = 0;    // raw ODB++ coordinates
    double size = 0;        // mm (slot: width)
    double slotLength = 0;  // mm, oval drill symbol (slot) in KiCad pad orientation; 0 = round
    enum Kind { PLATED, NON_PLATED, VIA } kind = PLATED;
};

// Look up a feature's attribute value by attribute name ("" if absent)
static std::string featureAttr(const OdbFeatures& feats, const std::map<int, std::string>& attrs,
                               const std::string& name) {
    for (auto& [idx, val] : attrs) {
        auto it = feats.attrDefs.find(idx);
        if (it != feats.attrDefs.end() && it->second == name) return val;
    }
    return "";
}

// Classify a drill hit. ODB++ .drill is an option attribute: 0 = plated, 1 = non_plated, 2 = via
// (text values are accepted too). Without it, a non-plated layer name or a .geometry VIA* string
// decides; everything else is plated.
static DrillHit::Kind classifyDrill(const OdbFeatures& feats, const std::map<int, std::string>& attrs,
                                    bool layerNonPlated) {
    std::string d = toLower(trim(featureAttr(feats, attrs, ".drill")));
    if (d == "0" || d == "plated") return DrillHit::PLATED;
    if (d == "1" || d == "non_plated") return DrillHit::NON_PLATED;
    if (d == "2" || d == "via") return DrillHit::VIA;
    if (layerNonPlated) return DrillHit::NON_PLATED;
    std::string geom = featureAttr(feats, attrs, ".geometry");
    if (!geom.empty()) {
        int strIdx = -1;
        try { strIdx = std::stoi(geom); } catch (...) {}
        auto it = feats.strings.find(strIdx);
        if (it != feats.strings.end() && it->second.find("VIA") != std::string::npos) return DrillHit::VIA;
    }
    return DrillHit::PLATED;
}

// KiCad orientation (degrees, CCW as seen from the top) of a P record. Orient 8 is a clockwise
// angle; 9 is the mirrored form, whose angle KiCad's exporter writes counter-clockwise
// (kicad-cli 9.0.9: a bottom pad at 30 deg exports as "9 30.0"). 0..7 are the legacy
// 0/90/180/270 clockwise steps, 4..7 mirrored.
static double padAngleKicad(const OdbPad& p) {
    double a;
    if (p.mirror == 9) a = p.angle;
    else if (p.mirror == 8) a = -p.angle;
    else if (p.mirror >= 4 && p.mirror <= 7) a = (p.mirror - 4) * 90.0;
    else a = -(p.mirror % 4) * 90.0;
    a = std::fmod(a, 360.0);
    if (a < 0) a += 360.0;
    if (std::abs(a - 360.0) < 1e-9) a = 0;
    return a;
}

// A stroked rectangle centred at (cx, cy) (KiCad coords): a gr_rect when axis-aligned, else four
// gr_lines. angle is KiCad CCW degrees.
static void emitStrokedRect(KicadPcb& pcb, double cx, double cy, double w, double h, double angle,
                            double stroke, const std::string& layer) {
    double q = std::fmod(angle, 90.0);
    if (std::abs(q) < 1e-6 || std::abs(q - 90.0) < 1e-6) {
        if (std::lround(angle / 90.0) % 2 == 1) std::swap(w, h);
        KicadGrRect r;
        r.x1 = cx - w / 2; r.y1 = cy - h / 2;
        r.x2 = cx + w / 2; r.y2 = cy + h / 2;
        r.width = stroke;
        r.type = "default";
        r.layer = layer;
        pcb.grRects.push_back(r);
        return;
    }
    double c = std::cos(degToRad(angle)), s = std::sin(degToRad(angle));
    double corners[4][2] = {{-w / 2, -h / 2}, {w / 2, -h / 2}, {w / 2, h / 2}, {-w / 2, h / 2}};
    double pts[4][2];
    for (int i = 0; i < 4; i++) {
        // KiCad rotation (Y down, CCW on screen)
        pts[i][0] = cx + corners[i][0] * c + corners[i][1] * s;
        pts[i][1] = cy - corners[i][0] * s + corners[i][1] * c;
    }
    for (int i = 0; i < 4; i++) {
        KicadGrLine gl;
        gl.x1 = pts[i][0]; gl.y1 = pts[i][1];
        gl.x2 = pts[(i + 1) % 4][0]; gl.y2 = pts[(i + 1) % 4][1];
        gl.width = stroke;
        gl.type = "default";
        gl.layer = layer;
        pcb.grLines.push_back(gl);
    }
}

KicadPcb transformToKicad(const OdbDesign& design) {
    KicadPcb pcb;

    // Determine unit scale factor (ODB++ can be INCH or MM, KiCad uses MM)
    // Check various sources for unit specification
    double unitScale = 1.0;  // Default: assume MM

    // Check component units first (most reliable for component positions)
    if (!design.topComponents.units.empty()) {
        unitScale = isInchUnits(design.topComponents.units) ? 25.4 : 1.0;
    } else if (!design.botComponents.units.empty()) {
        unitScale = isInchUnits(design.botComponents.units) ? 25.4 : 1.0;
    } else {
        // Check layer features for units
        for (auto& [name, features] : design.layerFeatures) {
            if (!features.units.empty()) {
                unitScale = isInchUnits(features.units) ? 25.4 : 1.0;
                break;
            }
        }
    }

    // If still no units found, check if coordinates look like inches
    // (values typically < 20 for inch boards, > 50 for mm boards)
    if (unitScale == 1.0 && !design.topComponents.components.empty()) {
        auto& comp = design.topComponents.components[0];
        if (comp.x > 0 && comp.x < 20 && comp.y > 0 && comp.y < 20) {
            unitScale = 25.4;  // Likely inches
        }
    }

    // Build layer name mapping (handles both KiCad and non-KiCad ODB++ sources)
    auto layerMap = buildLayerMap(design.layers);

    // Coordinate transforms with unit scaling (ODB++ INCH -> KiCad MM)
    auto scaleX = [unitScale](double x) { return x * unitScale; };
    auto scaleY = [unitScale](double y) { return -y * unitScale; };  // Also negate Y for KiCad
    auto scaleLen = [unitScale](double len) { return len * unitScale; };  // For widths, sizes

    // 1. Map layers
    for (auto& odbLayer : design.layers) {
        std::string nameLower = toLower(odbLayer.name);
        auto it = layerMap.find(nameLower);
        if (it == layerMap.end()) continue;
        auto* mapping = it->second;
        // Skip dielectric, component meta-layers
        std::string typeUpper = toUpper(odbLayer.type);
        if (typeUpper == "DIELECTRIC" || typeUpper == "COMPONENT") continue;

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

    // Feature -> net map from eda/data FID records: layer (lowercase) -> feature ordinal -> EDA net index.
    // This is the net the source design gave each copper feature; geometry-based guesses below are
    // only a fallback for files without it.
    std::map<std::string, std::map<int, int>> fidNets;
    for (auto& f : design.edaData.fids) {
        if (f.layerIdx < 0 || f.layerIdx >= (int)design.edaData.layers.size()) continue;
        fidNets[toLower(design.edaData.layers[f.layerIdx])][f.featIdx] = f.netIdx;
    }
    // KiCad net id of a feature from its FID record; -1 when the feature has none
    auto fidNet = [&](const std::string& layerLower, int featIdx) -> int {
        auto lit = fidNets.find(layerLower);
        if (featIdx < 0 || lit == fidNets.end()) return -1;
        auto fit = lit->second.find(featIdx);
        if (fit == lit->second.end()) return -1;
        return resolveNet(fit->second).first;
    };
    auto netNameOf = [&](int netId) -> std::string {
        for (auto& net : pcb.nets) {
            if (net.id == netId) return net.name;
        }
        return "";
    };

    // Helper: ODB layer name -> KiCad canonical name
    auto mapLayerName = [&layerMap](const std::string& odbName) -> std::string {
        auto it = layerMap.find(toLower(odbName));
        if (it != layerMap.end()) return it->second->canonical;
        return odbName;
    };

    // Feature lookup tolerance in raw ODB++ units (5 um)
    const double posTol = 0.005 / unitScale;
    auto near = [posTol](double ax, double ay, double bx, double by) {
        return std::abs(ax - bx) < posTol && std::abs(ay - by) < posTol;
    };

    // ODB++ layer name (lowercase) for a KiCad layer id, empty if the design has none
    auto odbLayerFor = [&layerMap](int kicadId) -> std::string {
        for (auto& [name, m] : layerMap) {
            if (m->id == kicadId) return name;
        }
        return "";
    };

    // Pad feature (flash) on an ODB++ layer at a point, nullptr if none
    auto padFeatureAt = [&](const std::string& odbLayer, double x, double y,
                            const OdbSymbol** symOut) -> const OdbPad* {
        auto it = design.layerFeatures.find(odbLayer);
        if (odbLayer.empty() || it == design.layerFeatures.end()) return nullptr;
        for (auto& p : it->second.pads) {
            if (near(p.x, p.y, x, y)) {
                if (symOut && p.symIdx >= 0 && p.symIdx < (int)it->second.symbols.size())
                    *symOut = &it->second.symbols[p.symIdx];
                return &p;
            }
        }
        return nullptr;
    };

    // Drill hits, classified per hit. The .drill attribute says plated / non_plated / via;
    // without it the layer name decides (KiCad: drill_non-plated_*), then .geometry VIA*.
    std::set<std::string> drillLayerNames;
    for (auto& l : design.layers) {
        if (toUpper(l.type) == "DRILL") drillLayerNames.insert(toLower(l.name));
    }
    for (auto& [name, f] : design.layerFeatures) {
        if (name.find("drill") != std::string::npos) drillLayerNames.insert(name);
    }
    std::vector<DrillHit> drillHits;
    for (auto& dlName : drillLayerNames) {
        auto it = design.layerFeatures.find(dlName);
        if (it == design.layerFeatures.end()) continue;
        auto& feats = it->second;
        bool layerNonPlated = dlName.find("non-plated") != std::string::npos ||
                              dlName.find("non_plated") != std::string::npos ||
                              dlName.find("npth") != std::string::npos ||
                              dlName.find("unplated") != std::string::npos;
        for (auto& p : feats.pads) {
            DrillHit h;
            h.x = p.x;
            h.y = p.y;
            if (p.symIdx >= 0 && p.symIdx < (int)feats.symbols.size()) {
                auto& ds = feats.symbols[p.symIdx];
                h.size = scaleLen((ds.shape == OdbSymbol::ROUND) ? ds.diameter : ds.width);
                if (ds.shape == OdbSymbol::OVAL && std::abs(ds.width - ds.height) > 1e-6) {
                    // slot: KiCad wants (drill oval <x> <y>) in the pad's frame
                    double w = scaleLen(ds.width), hh = scaleLen(ds.height);
                    if (std::lround(padAngleKicad(p) / 90.0) % 2 == 1) std::swap(w, hh);
                    h.size = w;
                    h.slotLength = hh;
                }
            }
            h.kind = classifyDrill(feats, p.attrs, layerNonPlated);
            drillHits.push_back(h);
        }
    }
    auto drillAt = [&](double x, double y) -> const DrillHit* {
        for (auto& h : drillHits) {
            if (near(h.x, h.y, x, y)) return &h;
        }
        return nullptr;
    };

    const std::string odbFCu = odbLayerFor(0), odbBCu = odbLayerFor(31);
    const std::string odbFMask = odbLayerFor(39), odbBMask = odbLayerFor(38);
    const std::string odbFPaste = odbLayerFor(35), odbBPaste = odbLayerFor(34);
    auto layerPresent = [&](const std::string& odbLayer) {
        return !odbLayer.empty() && design.layerFeatures.count(odbLayer) > 0;
    };
    // Mask/paste openings are read from the mask/paste layers when the design has them;
    // a design without them gets KiCad's defaults for the pad type.
    const bool haveMask = layerPresent(odbFMask) || layerPresent(odbBMask);
    // An opening at a point: a pad flash there, or (for custom-pad anchors, whose copper is a
    // 0.005 mm flash plus a surface) a positive surface covering the point.
    auto openingAt = [&](const std::string& odbLayer, double x, double y, bool anchorOnly) -> bool {
        if (padFeatureAt(odbLayer, x, y, nullptr)) return true;
        if (!anchorOnly) return false;
        auto it = design.layerFeatures.find(odbLayer);
        if (odbLayer.empty() || it == design.layerFeatures.end()) return false;
        for (auto& srf : it->second.surfaces) {
            if (srf.polarity == "N") continue;
            int depth = 0;
            for (auto& c : srf.contours) {
                bool in = false;
                size_t n = c.points.size();
                for (size_t i = 0, j = n - 1; i < n; j = i++) {
                    auto& a = c.points[i];
                    auto& b = c.points[j];
                    if (((a.y > y) != (b.y > y)) && (x < (b.x - a.x) * (y - a.y) / (b.y - a.y) + a.x)) in = !in;
                }
                if (in) depth += c.isHole ? -1 : 1;
            }
            if (depth > 0) return true;
        }
        return false;
    };
    const bool havePaste = layerPresent(odbFPaste) || layerPresent(odbBPaste);

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
            fp.x = scaleX(comp.x);
            fp.y = scaleY(comp.y);
            // Set rotation to 0 - use direct coordinate offsets instead of rotation math
            fp.angle = 0;
            fp.refdes = comp.refdes;
            fp.value = comp.properties.count("Value") ? comp.properties.at("Value") : "";

            // .comp_mount_type only sets the footprint attribute; each pad's type comes
            // from its own records (drill hit at the pad centre).
            if (comp.mountType == 1) fp.attr = "smd";
            else if (comp.mountType == 2) fp.attr = "through_hole";
            else fp.attr = "smd"; // default

            const std::string& sideCu = isBottom ? odbBCu : odbFCu;
            const std::string& otherCu = isBottom ? odbFCu : odbBCu;

            // Process terminals -> pads
            for (auto& term : comp.terminals) {
                KicadPad pad;
                pad.pinNum = term.pinNum;

                // Pad shape from the copper flash at the terminal: component side first,
                // then the far side (an SMD pad can sit on the opposite layer).
                const OdbSymbol* symPtr = nullptr;
                bool onSide = padFeatureAt(sideCu, term.x, term.y, &symPtr) != nullptr;
                bool onOther = false;
                if (!onSide) onOther = padFeatureAt(otherCu, term.x, term.y, &symPtr) != nullptr;
                OdbSymbol padSym;
                padSym.shape = OdbSymbol::ROUNDRECT; // default
                if (symPtr) padSym = *symPtr;

                switch (padSym.shape) {
                    case OdbSymbol::ROUND: pad.shape = "circle"; break;
                    case OdbSymbol::RECT: pad.shape = "rect"; break;
                    case OdbSymbol::ROUNDRECT: pad.shape = "roundrect"; break;
                    case OdbSymbol::OVAL: pad.shape = "oval"; break;
                    case OdbSymbol::SQUARE: pad.shape = "rect"; break;
                    default: pad.shape = "roundrect"; break;
                }

                pad.width = scaleLen(padSym.width);
                pad.height = scaleLen(padSym.height);
                // Ensure minimum pad size to avoid KiCad warnings
                if (pad.width < 0.001) pad.width = 0.1;
                if (pad.height < 0.001) pad.height = 0.1;
                if (padSym.shape == OdbSymbol::ROUNDRECT && padSym.cornerRadius > 0) {
                    double minDim = std::min(padSym.width, padSym.height);
                    pad.roundrectRatio = (minDim > 0) ? padSym.cornerRadius / minDim : 0;
                }

                // Pad position: direct offset with Y-flip, no rotation math
                // Since footprint rotation is 0, pad local position = world offset from component
                double dx = term.x - comp.x;
                double dy = term.y - comp.y;
                pad.x = scaleLen(dx);
                pad.y = -scaleLen(dy);  // Negate Y for KiCad's Y-down coordinate system

                // Pad type from the drill hit at the pad centre
                const DrillHit* hit = drillAt(term.x, term.y);
                if (hit && hit->kind == DrillHit::PLATED) {
                    pad.type = "thru_hole";
                    pad.drill = hit->size;
                    pad.drillHeight = hit->slotLength;
                } else if (hit && hit->kind == DrillHit::NON_PLATED) {
                    // NPTH: KiCad exports it as a 0.01 mm copper placeholder flash plus the
                    // non-plated hit. The hole is the pad; a real copper flash larger than the
                    // hole keeps its size.
                    pad.type = "np_thru_hole";
                    pad.drill = hit->size;
                    double flash = symPtr ? scaleLen(std::max(padSym.width, padSym.height)) : 0;
                    if (flash < hit->size) {
                        pad.shape = "circle";
                        pad.width = pad.height = hit->size;
                        pad.roundrectRatio = 0;
                    }
                } else {
                    pad.type = "smd";
                }

                // Pad layers: copper from the pad type / flash side, mask and paste from
                // the openings present on the mask/paste layers at the pad centre
                bool padOnBottom = (pad.type == "smd") ? (isBottom != onOther) : isBottom;
                if (pad.type == "thru_hole" || pad.type == "np_thru_hole") {
                    pad.layerNames = {"*.Cu"};
                } else {
                    pad.layerNames = {padOnBottom ? "B.Cu" : "F.Cu"};
                }
                bool th = pad.type == "thru_hole" || pad.type == "np_thru_hole";
                // custom-shape pad: no copper flash (or a sub-0.02 mm anchor), its shape is a surface
                bool anchor = !th && (!symPtr || std::max(padSym.width, padSym.height) * unitScale < 0.02);
                if (havePaste) {
                    if ((th || !padOnBottom) && openingAt(odbFPaste, term.x, term.y, anchor))
                        pad.layerNames.push_back("F.Paste");
                    if ((th || padOnBottom) && openingAt(odbBPaste, term.x, term.y, anchor))
                        pad.layerNames.push_back("B.Paste");
                } else if (!th) {
                    pad.layerNames.push_back(padOnBottom ? "B.Paste" : "F.Paste");
                }
                if (haveMask) {
                    if ((th || !padOnBottom) && openingAt(odbFMask, term.x, term.y, anchor))
                        pad.layerNames.push_back("F.Mask");
                    if ((th || padOnBottom) && openingAt(odbBMask, term.x, term.y, anchor))
                        pad.layerNames.push_back("B.Mask");
                } else {
                    pad.layerNames.push_back(th ? "*.Mask" : (padOnBottom ? "B.Mask" : "F.Mask"));
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

    // Build a set of component terminal positions to exclude from via detection
    // This prevents thermal vias/drills under pads from being created as standalone vias
    std::set<std::pair<int, int>> terminalPositions;  // scaled to 0.01mm precision
    auto addTerminals = [&](const OdbComponents& comps) {
        for (auto& comp : comps.components) {
            for (auto& term : comp.terminals) {
                int ix = static_cast<int>(std::round(term.x * 100));
                int iy = static_cast<int>(std::round(term.y * 100));
                terminalPositions.insert({ix, iy});
            }
        }
    };
    addTerminals(design.topComponents);
    addTerminals(design.botComponents);

    // 4. Detect vias from drill layer features
    // Build a set of via positions from drill layers
    struct ViaInfo {
        double x, y;           // ODB++ coordinates
        double drillDiameter;  // from drill layer symbol
        double padSize;        // from copper layer symbol
        int netId;
        bool netFromFid;       // net taken from the drill hit's FID record
    };
    std::vector<ViaInfo> viaInfos;

    for (auto& [layerNameLower, features] : design.layerFeatures) {
        // Find drill layers (name contains "drill")
        if (layerNameLower.find("drill") == std::string::npos) continue;

        for (auto& pad : features.pads) {
            // Skip drill features at component terminal positions (thermal vias under pads)
            int ix = static_cast<int>(std::round(pad.x * 100));
            int iy = static_cast<int>(std::round(pad.y * 100));
            if (terminalPositions.count({ix, iy})) continue;

            // Check if this drill pad has a .geometry attribute referencing a VIA string
            bool isVia = false;
            for (auto& [attrIdx, attrVal] : pad.attrs) {
                auto attrDefIt = features.attrDefs.find(attrIdx);
                if (attrDefIt != features.attrDefs.end() && attrDefIt->second == ".geometry") {
                    // The value is a string index; look up the string
                    int strIdx = -1;
                    try { strIdx = std::stoi(attrVal); } catch (...) {}
                    if (strIdx >= 0) {
                        auto strIt = features.strings.find(strIdx);
                        if (strIt != features.strings.end() && strIt->second.find("VIA") != std::string::npos) {
                            isVia = true;
                        }
                    }
                }
            }
            if (!isVia) continue;

            ViaInfo vi;
            vi.x = pad.x;
            vi.y = pad.y;
            int viaFid = fidNet(layerNameLower, pad.featIdx);
            vi.netFromFid = viaFid >= 0;
            vi.netId = std::max(viaFid, 0);

            // Drill diameter from drill-layer symbol
            if (pad.symIdx < (int)features.symbols.size()) {
                auto& sym = features.symbols[pad.symIdx];
                vi.drillDiameter = scaleLen((sym.shape == OdbSymbol::ROUND) ? sym.diameter : sym.width);
            } else {
                vi.drillDiameter = 0.3; // default (mm)
            }

            // Find pad size from copper layer (F.Cu) at same position
            vi.padSize = vi.drillDiameter + 0.3; // default annular ring (already scaled)
            std::string fcuName;
            for (auto& [name, m] : layerMap) {
                if (m->id == 0) { fcuName = name; break; }
            }
            auto fcuIt = design.layerFeatures.find(fcuName);
            if (fcuIt != design.layerFeatures.end()) {
                for (auto& cuPad : fcuIt->second.pads) {
                    if (std::abs(cuPad.x - pad.x) < 0.01 && std::abs(cuPad.y - pad.y) < 0.01) {
                        if (cuPad.symIdx < (int)fcuIt->second.symbols.size()) {
                            auto& cuSym = fcuIt->second.symbols[cuPad.symIdx];
                            if (cuSym.shape == OdbSymbol::ROUND) {
                                vi.padSize = scaleLen(cuSym.diameter);
                            } else {
                                vi.padSize = scaleLen(std::max(cuSym.width, cuSym.height));
                            }
                        }
                        break;
                    }
                }
            }

            viaInfos.push_back(vi);
        }
    }

    // Assign nets to vias without an FID record from netlist points with 'v' flag
    for (auto& vi : viaInfos) {
        if (vi.netFromFid) continue;
        for (auto& pt : design.netlist.points) {
            if (!pt.isVia) continue;
            if (std::abs(pt.x - vi.x) < 0.01 && std::abs(pt.y - vi.y) < 0.01) {
                // Map netlist index to KiCad net
                for (auto& nd : design.netlist.nets) {
                    if (nd.index == pt.netIdx) {
                        auto it = netNameToId.find(nd.name);
                        if (it != netNameToId.end()) {
                            vi.netId = it->second;
                        }
                        break;
                    }
                }
                break;
            }
        }
        // If still no net, try to find from any netlist point at this location
        if (vi.netId == 0) {
            for (auto& pt : design.netlist.points) {
                if (std::abs(pt.x - vi.x) < 0.01 && std::abs(pt.y - vi.y) < 0.01) {
                    for (auto& nd : design.netlist.nets) {
                        if (nd.index == pt.netIdx) {
                            auto it = netNameToId.find(nd.name);
                            if (it != netNameToId.end()) {
                                vi.netId = it->second;
                                break;
                            }
                        }
                    }
                    if (vi.netId > 0) break;
                }
            }
        }
    }

    // Build a set of via positions for quick lookup (in KiCad coords)
    // to exclude them from segment generation
    struct PosKey {
        double x, y;
    };
    std::vector<PosKey> viaPositions;
    for (auto& vi : viaInfos) {
        viaPositions.push_back({vi.x, vi.y});
    }

    auto isViaPosition = [&](double odbX, double odbY) -> bool {
        for (auto& vp : viaPositions) {
            if (std::abs(vp.x - odbX) < 0.01 && std::abs(vp.y - odbY) < 0.01) return true;
        }
        return false;
    };

    // Emit vias
    for (auto& vi : viaInfos) {
        KicadVia kv;
        kv.x = scaleX(vi.x);
        kv.y = scaleY(vi.y);
        kv.size = vi.padSize;
        kv.drill = vi.drillDiameter;
        kv.fromLayer = "F.Cu";
        kv.toLayer = "B.Cu";
        kv.netId = vi.netId;
        pcb.vias.push_back(kv);
    }

    // 5. Build pad lookup table for net assignment (absolute positions)
    struct PadNetInfo {
        double x, y;  // absolute KiCad coordinates
        int netId;
        bool throughHole; // if true, pad exists on all copper layers
    };
    std::vector<PadNetInfo> padLookup;

    for (auto& fp : pcb.footprints) {
        double angleRad = degToRad(fp.angle);
        double cosA = std::cos(angleRad);
        double sinA = std::sin(angleRad);
        for (auto& pad : fp.pads) {
            PadNetInfo pni;
            pni.x = fp.x + pad.x * cosA - pad.y * sinA;
            pni.y = fp.y + pad.x * sinA + pad.y * cosA;
            pni.netId = pad.netId;
            pni.throughHole = (pad.type == "thru_hole");
            padLookup.push_back(pni);
        }
    }

    // Also add via positions to pad lookup for trace-to-via net propagation
    for (auto& vi : viaInfos) {
        PadNetInfo pni;
        pni.x = scaleX(vi.x);
        pni.y = scaleY(vi.y);
        pni.netId = vi.netId;
        pni.throughHole = true; // vias span all copper layers
        padLookup.push_back(pni);
    }

    // Helper to find net for a point on a given layer
    auto findNetAtPoint = [&](double kx, double ky, const std::string& layer) -> int {
        bool isCopperLayer = layer.size() > 3 && layer.compare(layer.size() - 3, 3, ".Cu") == 0;
        if (!isCopperLayer) return 0;

        for (auto& pni : padLookup) {
            // TH pads match any copper layer; SMD pads always match too, since they were
            // placed from the correct footprint layer.
            if (std::abs(pni.x - kx) < 0.02 && std::abs(pni.y - ky) < 0.02) {
                if (pni.netId > 0) return pni.netId;
            }
        }
        return 0;
    };

    // 6. Transform copper traces
    for (auto& [layerNameLower, features] : design.layerFeatures) {
        auto lmIt = layerMap.find(layerNameLower);
        if (lmIt == layerMap.end()) continue;
        auto* mapping = lmIt->second;
        // Only copper layers (F.Cu=0, B.Cu=31, inner=1-30)
        if (mapping->id > 31) continue;

        std::string kicadLayer = mapping->canonical;

        for (auto& line : features.lines) {
            KicadSegment seg;
            seg.x1 = scaleX(line.x1);
            seg.y1 = scaleY(line.y1);
            seg.x2 = scaleX(line.x2);
            seg.y2 = scaleY(line.y2);

            // Width from symbol
            if (line.symIdx < (int)features.symbols.size()) {
                auto& sym = features.symbols[line.symIdx];
                if (sym.shape == OdbSymbol::ROUND) {
                    seg.width = scaleLen(sym.diameter);
                } else {
                    seg.width = scaleLen(sym.width);
                }
            }

            seg.layer = kicadLayer;

            // Net: the FID record, else check both endpoints against pad and via positions
            int segFid = fidNet(layerNameLower, line.featIdx);
            seg.netId = segFid >= 0 ? segFid : findNetAtPoint(seg.x1, seg.y1, kicadLayer);
            if (seg.netId == 0 && segFid < 0) {
                seg.netId = findNetAtPoint(seg.x2, seg.y2, kicadLayer);
            }

            pcb.segments.push_back(seg);
        }

        // Process arcs on copper layers
        for (auto& arc : features.arcs) {
            KicadArc ka;
            ka.xs = scaleX(arc.xs);
            ka.ys = scaleY(arc.ys);
            ka.xe = scaleX(arc.xe);
            ka.ye = scaleY(arc.ye);

            // Compute midpoint
            double xm_odb, ym_odb;
            computeArcMidpoint(arc.xs, arc.ys, arc.xe, arc.ye,
                               arc.xc, arc.yc, arc.clockwise, xm_odb, ym_odb);
            ka.xm = scaleX(xm_odb);
            ka.ym = scaleY(ym_odb);

            // Width from symbol
            if (arc.symIdx < (int)features.symbols.size()) {
                auto& sym = features.symbols[arc.symIdx];
                if (sym.shape == OdbSymbol::ROUND) {
                    ka.width = scaleLen(sym.diameter);
                } else {
                    ka.width = scaleLen(sym.width);
                }
            }

            ka.layer = kicadLayer;

            // Net: the FID record, else check endpoints against pad and via positions
            int arcFid = fidNet(layerNameLower, arc.featIdx);
            ka.netId = arcFid >= 0 ? arcFid : findNetAtPoint(ka.xs, ka.ys, kicadLayer);
            if (ka.netId == 0 && arcFid < 0) {
                ka.netId = findNetAtPoint(ka.xe, ka.ye, kicadLayer);
            }

            pcb.arcs.push_back(ka);
        }
    }

    // Propagate nets along connected traces (multi-hop)
    // Traces sharing an endpoint inherit each other's net
    auto touches = [](double ax, double ay, double bx, double by) {
        return std::abs(ax - bx) < 0.02 && std::abs(ay - by) < 0.02;
    };

    bool changed = true;
    while (changed) {
        changed = false;
        for (size_t i = 0; i < pcb.segments.size(); i++) {
            if (pcb.segments[i].netId > 0) continue;
            for (size_t j = 0; j < pcb.segments.size(); j++) {
                if (i == j || pcb.segments[j].netId == 0) continue;
                if (pcb.segments[i].layer != pcb.segments[j].layer) continue;
                // Check if they share an endpoint
                if (touches(pcb.segments[i].x1, pcb.segments[i].y1, pcb.segments[j].x1, pcb.segments[j].y1) ||
                    touches(pcb.segments[i].x1, pcb.segments[i].y1, pcb.segments[j].x2, pcb.segments[j].y2) ||
                    touches(pcb.segments[i].x2, pcb.segments[i].y2, pcb.segments[j].x1, pcb.segments[j].y1) ||
                    touches(pcb.segments[i].x2, pcb.segments[i].y2, pcb.segments[j].x2, pcb.segments[j].y2)) {
                    pcb.segments[i].netId = pcb.segments[j].netId;
                    changed = true;
                    break;
                }
            }
        }
    }

    // Propagate nets from segments back to vias
    for (auto& via : pcb.vias) {
        if (via.netId > 0) continue;
        for (auto& seg : pcb.segments) {
            if (seg.netId == 0) continue;
            if (touches(via.x, via.y, seg.x1, seg.y1) ||
                touches(via.x, via.y, seg.x2, seg.y2)) {
                via.netId = seg.netId;
                break;
            }
        }
    }

    // Also propagate from arcs to vias
    for (auto& via : pcb.vias) {
        if (via.netId > 0) continue;
        for (auto& arc : pcb.arcs) {
            if (arc.netId == 0) continue;
            if (touches(via.x, via.y, arc.xs, arc.ys) ||
                touches(via.x, via.y, arc.xe, arc.ye)) {
                via.netId = arc.netId;
                break;
            }
        }
    }

    // Propagate nets through vias (cross-layer propagation)
    // If a via has a net, segments on any copper layer touching it get that net
    changed = true;
    while (changed) {
        changed = false;
        for (auto& seg : pcb.segments) {
            if (seg.netId > 0) continue;
            for (auto& via : pcb.vias) {
                if (via.netId == 0) continue;
                if (touches(via.x, via.y, seg.x1, seg.y1) ||
                    touches(via.x, via.y, seg.x2, seg.y2)) {
                    seg.netId = via.netId;
                    changed = true;
                    break;
                }
            }
        }
        // Also propagate between segments on same layer again
        for (size_t i = 0; i < pcb.segments.size(); i++) {
            if (pcb.segments[i].netId > 0) continue;
            for (size_t j = 0; j < pcb.segments.size(); j++) {
                if (i == j || pcb.segments[j].netId == 0) continue;
                if (pcb.segments[i].layer != pcb.segments[j].layer) continue;
                if (touches(pcb.segments[i].x1, pcb.segments[i].y1, pcb.segments[j].x1, pcb.segments[j].y1) ||
                    touches(pcb.segments[i].x1, pcb.segments[i].y1, pcb.segments[j].x2, pcb.segments[j].y2) ||
                    touches(pcb.segments[i].x2, pcb.segments[i].y2, pcb.segments[j].x1, pcb.segments[j].y1) ||
                    touches(pcb.segments[i].x2, pcb.segments[i].y2, pcb.segments[j].x2, pcb.segments[j].y2)) {
                    pcb.segments[i].netId = pcb.segments[j].netId;
                    changed = true;
                    break;
                }
            }
        }
    }

    // Final pass: propagate from segments back to vias again
    for (auto& via : pcb.vias) {
        if (via.netId > 0) continue;
        for (auto& seg : pcb.segments) {
            if (seg.netId == 0) continue;
            if (touches(via.x, via.y, seg.x1, seg.y1) ||
                touches(via.x, via.y, seg.x2, seg.y2)) {
                via.netId = seg.netId;
                break;
            }
        }
    }

    // 7. Board outline from profile
    // Helper to emit contour lines on Edge.Cuts
    auto emitContourAsEdgeCuts = [&](const OdbContour& contour) {
        auto& pts = contour.points;
        if (pts.size() < 4) return;

        // Collect unique points (skip OB starting point duplicate and closing duplicate)
        std::vector<OdbPoint> unique;
        for (size_t i = 1; i < pts.size(); i++) {
            if (i == pts.size() - 1 &&
                std::abs(pts[i].x - pts[1].x) < 0.001 &&
                std::abs(pts[i].y - pts[1].y) < 0.001)
                continue;
            unique.push_back(pts[i]);
        }

        // Check if rectangular (only for non-holes)
        if (!contour.isHole && unique.size() == 4) {
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
                rect.x1 = scaleX(minX);
                rect.y1 = scaleY(maxY);
                rect.x2 = scaleX(maxX);
                rect.y2 = scaleY(minY);
                rect.width = 0.05;
                rect.type = "default";
                rect.layer = "Edge.Cuts";
                pcb.grRects.push_back(rect);
                return;
            }
        }

        // Non-rectangular or hole: emit individual line segments
        for (size_t i = 1; i < pts.size() - 1; i++) {
            KicadGrLine gl;
            gl.x1 = scaleX(pts[i].x);
            gl.y1 = scaleY(pts[i].y);
            gl.x2 = scaleX(pts[i + 1].x);
            gl.y2 = scaleY(pts[i + 1].y);
            gl.width = 0.05;
            gl.type = "default";
            gl.layer = "Edge.Cuts";
            pcb.grLines.push_back(gl);
        }
    };

    // The outline is drawn once: from the Edge.Cuts layer when the design has one with features
    // (KiCad exports both, and the layer keeps the arcs the profile flattens), else from the profile.
    std::string edgeLayer = odbLayerFor(44);
    if (edgeLayer.empty()) edgeLayer = "edge.cuts";
    bool edgeLayerHasFeatures = false;
    {
        auto it = design.layerFeatures.find(edgeLayer);
        if (it != design.layerFeatures.end()) {
            auto& f = it->second;
            edgeLayerHasFeatures = !f.lines.empty() || !f.arcs.empty() || !f.pads.empty() || !f.surfaces.empty();
        }
    }
    if (!edgeLayerHasFeatures && !design.profile.surfaces.empty()) {
        for (auto& surface : design.profile.surfaces) {
            for (auto& contour : surface.contours) {
                emitContourAsEdgeCuts(contour);
            }
        }
    }

    // 8. Edge.Cuts layer features (lines, arcs, circles from features file)
    {
        auto ecIt = design.layerFeatures.find(edgeLayer);
        if (ecIt != design.layerFeatures.end()) {
            auto& ecFeats = ecIt->second;
            for (auto& line : ecFeats.lines) {
                KicadGrLine gl;
                gl.x1 = scaleX(line.x1);
                gl.y1 = scaleY(line.y1);
                gl.x2 = scaleX(line.x2);
                gl.y2 = scaleY(line.y2);
                if (line.symIdx < (int)ecFeats.symbols.size()) {
                    auto& sym = ecFeats.symbols[line.symIdx];
                    gl.width = scaleLen((sym.shape == OdbSymbol::ROUND) ? sym.diameter : sym.width);
                } else {
                    gl.width = 0.05;
                }
                gl.type = "default";
                gl.layer = "Edge.Cuts";
                pcb.grLines.push_back(gl);
            }
            for (auto& arc : ecFeats.arcs) {
                KicadGrArc ga;
                ga.xs = scaleX(arc.xs);
                ga.ys = scaleY(arc.ys);
                ga.xe = scaleX(arc.xe);
                ga.ye = scaleY(arc.ye);
                double xm_odb, ym_odb;
                computeArcMidpoint(arc.xs, arc.ys, arc.xe, arc.ye,
                                   arc.xc, arc.yc, arc.clockwise, xm_odb, ym_odb);
                ga.xm = scaleX(xm_odb);
                ga.ym = scaleY(ym_odb);
                if (arc.symIdx < (int)ecFeats.symbols.size()) {
                    auto& sym = ecFeats.symbols[arc.symIdx];
                    ga.width = scaleLen((sym.shape == OdbSymbol::ROUND) ? sym.diameter : sym.width);
                } else {
                    ga.width = 0.05;
                }
                ga.layer = "Edge.Cuts";
                pcb.grArcs.push_back(ga);
            }
            // Donut/circle pads on Edge.Cuts → circular cutouts via gr_circle
            // Emit as a series of arc segments approximating a circle
            for (auto& pad : ecFeats.pads) {
                if (pad.symIdx < (int)ecFeats.symbols.size()) {
                    auto& sym = ecFeats.symbols[pad.symIdx];
                    // donut_rc<w>x<h>x<lw>xr<r>: a stroked rectangle (KiCad's gr_rect). The
                    // symbol's outer box is the rectangle plus the stroke width.
                    if (sym.donut && sym.shape == OdbSymbol::ROUNDRECT) {
                        emitStrokedRect(pcb, scaleX(pad.x), scaleY(pad.y), scaleLen(sym.width - sym.lineWidth),
                                        scaleLen(sym.height - sym.lineWidth), padAngleKicad(pad),
                                        scaleLen(sym.lineWidth), "Edge.Cuts");
                        continue;
                    }
                    // For donut symbols, use the outer diameter to draw a circle
                    double radius = 0;
                    if (sym.donut && sym.shape == OdbSymbol::ROUND) {
                        // donut_r<outer>x<inner> - extract outer diameter
                        radius = sym.diameter / 2.0;
                        if (radius <= 0) radius = sym.width / 2.0;
                    } else if (sym.shape == OdbSymbol::ROUND) {
                        radius = sym.diameter / 2.0;
                    }
                    if (radius > 0) {
                        // Emit as gr_circle (KiCad supports it)
                        KicadGrArc ga;
                        // KiCad gr_circle: (gr_circle (center x y) (end x+r y) ...)
                        // We'll use a full arc: start at right, mid at top, end at left...
                        // Actually, better to emit 2 half arcs
                        double cx = scaleX(pad.x);
                        double cy = scaleY(pad.y);
                        double radiusMm = scaleLen(radius);
                        // First half: right -> top -> left
                        ga.xs = cx + radiusMm;
                        ga.ys = cy;
                        ga.xm = cx;
                        ga.ym = cy - radiusMm;
                        ga.xe = cx - radiusMm;
                        ga.ye = cy;
                        ga.width = 0.05;
                        ga.layer = "Edge.Cuts";
                        pcb.grArcs.push_back(ga);
                        // Second half: left -> bottom -> right
                        ga.xs = cx - radiusMm;
                        ga.ys = cy;
                        ga.xm = cx;
                        ga.ym = cy + radiusMm;
                        ga.xe = cx + radiusMm;
                        ga.ye = cy;
                        pcb.grArcs.push_back(ga);
                    }
                }
            }
        }
    }

    // Helper to check if a point is inside a polygon (ray casting)
    auto pointInPolygon = [](double px, double py,
                              const std::vector<std::pair<double,double>>& poly) -> bool {
        if (poly.size() < 3) return false;
        bool inside = false;
        size_t n = poly.size();
        for (size_t i = 0, j = n - 1; i < n; j = i++) {
            double xi = poly[i].first, yi = poly[i].second;
            double xj = poly[j].first, yj = poly[j].second;
            if (((yi > py) != (yj > py)) &&
                (px < (xj - xi) * (py - yi) / (yj - yi) + xi)) {
                inside = !inside;
            }
        }
        return inside;
    };

    // 9. Copper fill zones from surface records on copper layers
    for (auto& [layerNameLower, features] : design.layerFeatures) {
        auto lmIt = layerMap.find(layerNameLower);
        if (lmIt == layerMap.end()) continue;
        auto* mapping = lmIt->second;
        if (mapping->id > 31) continue; // copper only
        std::string kicadLayer = mapping->canonical;
        int layerId = mapping->id;

        for (auto& surface : features.surfaces) {
            KicadZone zone;
            zone.layer = kicadLayer;
            zone.netId = 0;
            zone.netName = "";

            // Helper to compute bounding box area for a polygon
            auto computeBBoxArea = [](const std::vector<std::pair<double,double>>& pts) -> double {
                if (pts.empty()) return 0;
                double minX = pts[0].first, maxX = pts[0].first;
                double minY = pts[0].second, maxY = pts[0].second;
                for (auto& [x, y] : pts) {
                    minX = std::min(minX, x);
                    maxX = std::max(maxX, x);
                    minY = std::min(minY, y);
                    maxY = std::max(maxY, y);
                }
                return (maxX - minX) * (maxY - minY);
            };

            // Collect all contours and find the largest non-hole for outline
            double largestArea = 0;
            for (auto& contour : surface.contours) {
                std::vector<std::pair<double,double>> polyPts;
                for (size_t i = 1; i < contour.points.size(); i++) {
                    // Skip closing duplicate
                    if (i == contour.points.size() - 1 && contour.points.size() > 2 &&
                        std::abs(contour.points[i].x - contour.points[1].x) < 0.001 &&
                        std::abs(contour.points[i].y - contour.points[1].y) < 0.001)
                        continue;
                    polyPts.push_back({scaleX(contour.points[i].x),
                                       scaleY(contour.points[i].y)});
                }

                // Use largest non-hole contour as the zone outline
                if (!contour.isHole) {
                    double area = computeBBoxArea(polyPts);
                    if (area > largestArea) {
                        largestArea = area;
                        zone.outlinePoints = polyPts;
                    }
                }
                zone.filledPolygons.push_back(polyPts);
            }

            // Zone net: the surface's FID record. Without one, fall back to a pad whose centre lies
            // in the fill itself (inside an island, outside its holes): a pad sitting in a clearance
            // hole of the pour belongs to another net.
            int zoneFid = fidNet(layerNameLower, surface.featIdx);
            if (zoneFid >= 0) {
                zone.netId = zoneFid;
            } else if (!zone.outlinePoints.empty()) {
                for (auto& pni : padLookup) {
                    // Check if pad is on this layer (TH pads on any copper, SMD only on their layer)
                    bool layerMatch = pni.throughHole || (layerId == 0) || (layerId == 31);
                    if (!layerMatch || pni.netId <= 0) continue;
                    int depth = 0;
                    for (size_t ci = 0; ci < surface.contours.size(); ci++) {
                        if (pointInPolygon(pni.x, pni.y, zone.filledPolygons[ci]))
                            depth += surface.contours[ci].isHole ? -1 : 1;
                    }
                    if (depth > 0) {
                        zone.netId = pni.netId;
                        break;
                    }
                }
            }
            zone.netName = netNameOf(zone.netId);

            if (!zone.outlinePoints.empty() || !zone.filledPolygons.empty()) {
                pcb.zones.push_back(zone);
            }
        }
    }

    // 10. Non-copper, non-edge.cuts layer graphics (silkscreen, fab, courtyard, etc.)
    // These become gr_line / gr_arc at board level
    // Build set of copper layer names (lowercase) from the layer map
    std::set<std::string> copperLayers;
    for (auto& [name, m] : layerMap) {
        if (m->id <= 31) copperLayers.insert(name);
    }
    std::set<std::string> skipLayers = {"edge.cuts", edgeLayer}; // already handled
    for (auto& [layerNameLower, features] : design.layerFeatures) {
        if (copperLayers.count(layerNameLower)) continue;
        if (skipLayers.count(layerNameLower)) continue;
        if (layerNameLower.find("drill") != std::string::npos) continue;
        if (layerNameLower.find("comp_") != std::string::npos) continue;
        // Skip layers with no mapping to KiCad
        auto lmIt2 = layerMap.find(layerNameLower);
        if (lmIt2 == layerMap.end()) continue;
        std::string kicadLayer = lmIt2->second->canonical;

        for (auto& line : features.lines) {
            KicadGrLine gl;
            gl.x1 = scaleX(line.x1);
            gl.y1 = scaleY(line.y1);
            gl.x2 = scaleX(line.x2);
            gl.y2 = scaleY(line.y2);
            if (line.symIdx < (int)features.symbols.size()) {
                auto& sym = features.symbols[line.symIdx];
                gl.width = scaleLen((sym.shape == OdbSymbol::ROUND) ? sym.diameter : sym.width);
            } else {
                gl.width = 0.1;
            }
            gl.type = "solid";
            gl.layer = kicadLayer;
            pcb.grLines.push_back(gl);
        }

        for (auto& arc : features.arcs) {
            KicadGrArc ga;
            ga.xs = scaleX(arc.xs);
            ga.ys = scaleY(arc.ys);
            ga.xe = scaleX(arc.xe);
            ga.ye = scaleY(arc.ye);
            double xm_odb, ym_odb;
            computeArcMidpoint(arc.xs, arc.ys, arc.xe, arc.ye,
                               arc.xc, arc.yc, arc.clockwise, xm_odb, ym_odb);
            ga.xm = scaleX(xm_odb);
            ga.ym = scaleY(ym_odb);
            if (arc.symIdx < (int)features.symbols.size()) {
                auto& sym = features.symbols[arc.symIdx];
                ga.width = scaleLen((sym.shape == OdbSymbol::ROUND) ? sym.diameter : sym.width);
            } else {
                ga.width = 0.1;
            }
            ga.layer = kicadLayer;
            pcb.grArcs.push_back(ga);
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
        out << "    (" << layer.id << " \"" << escapeString(layer.canonicalName) << "\" " << layer.type;
        if (!layer.alias.empty()) out << " \"" << escapeString(layer.alias) << "\"";
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
        out << "  (net " << net.id << " \"" << escapeString(net.name) << "\")\n";
    }

    // Footprints
    for (auto& fp : pcb.footprints) {
        out << "\n  (footprint \"" << escapeString(fp.library) << "\" (layer \"" << escapeString(fp.layer) << "\")\n";
        out << "    (tstamp " << fp.uuid << ")\n";
        out << "    (at " << formatFloat(fp.x) << " " << formatFloat(fp.y);
        if (std::abs(fp.angle) > 0.001) out << " " << formatFloat(fp.angle);
        out << ")\n";
        out << "    (property \"Reference\" \"" << escapeString(fp.refdes) << "\")\n";
        out << "    (property \"Value\" \"" << escapeString(fp.value) << "\")\n";
        out << "    (attr " << fp.attr << ")\n";

        // fp_text reference
        out << "    (fp_text reference \"" << escapeString(fp.refdes) << "\" (at 0 -1.65) (layer \"F.SilkS\")\n";
        out << "      (effects (font (size 1 1) (thickness 0.15)))\n";
        out << "    )\n";

        // fp_text value
        out << "    (fp_text value \"" << escapeString(fp.value) << "\" (at 0 1.65) (layer \"F.Fab\")\n";
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
            out << "    (pad \"" << escapeString(pad.pinNum) << "\" " << pad.type << " " << pad.shape;
            out << " (at " << formatFloat(pad.x) << " " << formatFloat(pad.y) << ")";
            out << " (size " << formatFloat(pad.width) << " " << formatFloat(pad.height) << ")";
            out << " (layers";
            for (auto& l : pad.layerNames) out << " \"" << escapeString(l) << "\"";
            out << ")";
            if (pad.shape == "roundrect" && pad.roundrectRatio > 0) {
                out << " (roundrect_rratio " << formatFloat(pad.roundrectRatio) << ")";
            }
            if (pad.drill > 0 && pad.drillHeight > 0) {
                out << " (drill oval " << formatFloat(pad.drill) << " " << formatFloat(pad.drillHeight) << ")";
            } else if (pad.drill > 0) {
                out << " (drill " << formatFloat(pad.drill) << ")";
            }
            if (pad.netId > 0) {
                out << " (net " << pad.netId << " \"" << escapeString(pad.netName) << "\")";
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
            << ") (layer \"" << escapeString(seg.layer)
            << "\") (net " << seg.netId << "))\n";
    }

    // Copper arcs
    for (auto& arc : pcb.arcs) {
        out << "\n  (arc (start " << formatFloat(arc.xs) << " " << formatFloat(arc.ys)
            << ") (mid " << formatFloat(arc.xm) << " " << formatFloat(arc.ym)
            << ") (end " << formatFloat(arc.xe) << " " << formatFloat(arc.ye)
            << ") (width " << formatFloat(arc.width)
            << ") (layer \"" << escapeString(arc.layer)
            << "\") (net " << arc.netId << "))\n";
    }

    // Vias
    for (auto& via : pcb.vias) {
        out << "\n  (via (at " << formatFloat(via.x) << " " << formatFloat(via.y)
            << ") (size " << formatFloat(via.size)
            << ") (drill " << formatFloat(via.drill)
            << ") (layers \"" << escapeString(via.fromLayer) << "\" \"" << escapeString(via.toLayer)
            << "\") (net " << via.netId << "))\n";
    }

    // Graphic lines
    for (auto& gl : pcb.grLines) {
        out << "\n  (gr_line (start " << formatFloat(gl.x1) << " " << formatFloat(gl.y1)
            << ") (end " << formatFloat(gl.x2) << " " << formatFloat(gl.y2)
            << ") (stroke (width " << formatFloat(gl.width)
            << ") (type " << gl.type << ")) (fill none) (layer \"" << escapeString(gl.layer) << "\"))\n";
    }

    // Graphic rects
    for (auto& gr : pcb.grRects) {
        out << "\n  (gr_rect (start " << formatFloat(gr.x1) << " " << formatFloat(gr.y1)
            << ") (end " << formatFloat(gr.x2) << " " << formatFloat(gr.y2)
            << ") (stroke (width " << formatFloat(gr.width)
            << ") (type " << gr.type << ")) (fill none) (layer \"" << escapeString(gr.layer) << "\"))\n";
    }

    // Graphic arcs
    for (auto& ga : pcb.grArcs) {
        out << "\n  (gr_arc (start " << formatFloat(ga.xs) << " " << formatFloat(ga.ys)
            << ") (mid " << formatFloat(ga.xm) << " " << formatFloat(ga.ym)
            << ") (end " << formatFloat(ga.xe) << " " << formatFloat(ga.ye)
            << ") (stroke (width " << formatFloat(ga.width)
            << ") (type solid)) (layer \"" << escapeString(ga.layer) << "\"))\n";
    }

    // Zones (modern KiCad 8.0+ format)
    int zoneIdx = 0;
    for (auto& zone : pcb.zones) {
        // Skip zones without outline points
        if (zone.outlinePoints.empty()) continue;

        out << "\n  (zone (net " << zone.netId << ") (net_name \"" << escapeString(zone.netName)
            << "\") (layer \"" << escapeString(zone.layer) << "\") (uuid \"" << makeUuid(10000 + zoneIdx++) << "\")\n";
        out << "    (hatch edge 0.5)\n";
        out << "    (connect_pads (clearance 0))\n";
        out << "    (min_thickness 0.25)\n";
        out << "    (filled_areas_thickness no)\n";
        out << "    (fill yes (thermal_gap 0.5) (thermal_bridge_width 0.5))\n";
        out << "    (polygon\n      (pts\n";
        for (auto& [x, y] : zone.outlinePoints) {
            out << "        (xy " << formatFloat(x) << " " << formatFloat(y) << ")\n";
        }
        out << "      )\n    )\n";
        // Include filled polygons so zones display immediately without refill
        for (auto& poly : zone.filledPolygons) {
            if (poly.size() < 3) continue;
            out << "    (filled_polygon\n      (layer \"" << escapeString(zone.layer) << "\")\n      (pts\n";
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
