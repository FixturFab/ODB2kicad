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

// Build layer name mapping from ODB++ matrix layer stack.
// Handles both KiCad-generated (F.CU, B.CU) and non-KiCad (SIGNAL_1, SIGNAL_10, etc.) naming.
static std::map<std::string, const LayerMapping*> buildLayerMap(const std::vector<OdbLayer>& layers) {
    std::map<std::string, const LayerMapping*> result;

    // First pass: try direct name matching (KiCad ODB++ output)
    for (auto& layer : layers) {
        auto* m = findLayerByOdbName(layer.name);
        if (m) {
            result[toLower(layer.name)] = m;
        }
    }

    // If we found F.Cu and B.Cu by name, we're done (KiCad source)
    bool hasFCu = false, hasBCu = false;
    for (auto& [name, m] : result) {
        if (m->id == 0) hasFCu = true;
        if (m->id == 31) hasBCu = true;
    }
    if (hasFCu && hasBCu) return result;

    // Second pass: TYPE-based mapping for non-KiCad ODB++ files
    // Collect copper layers (SIGNAL and POWER_GROUND) in row order
    std::vector<std::pair<std::string, int>> copperLayers; // name, row
    std::vector<std::pair<std::string, int>> maskLayers;
    std::vector<std::pair<std::string, int>> pasteLayers;
    std::vector<std::pair<std::string, int>> silkLayers;
    std::vector<std::pair<std::string, int>> drillLayers;

    for (auto& layer : layers) {
        std::string type = toUpper(layer.type);
        if (type == "SIGNAL" || type == "POWER_GROUND")
            copperLayers.push_back({layer.name, layer.row});
        else if (type == "SOLDER_MASK")
            maskLayers.push_back({layer.name, layer.row});
        else if (type == "SOLDER_PASTE")
            pasteLayers.push_back({layer.name, layer.row});
        else if (type == "SILK_SCREEN")
            silkLayers.push_back({layer.name, layer.row});
        else if (type == "DRILL")
            drillLayers.push_back({layer.name, layer.row});
    }

    // Sort by row
    auto byRow = [](auto& a, auto& b) { return a.second < b.second; };
    std::sort(copperLayers.begin(), copperLayers.end(), byRow);
    std::sort(maskLayers.begin(), maskLayers.end(), byRow);
    std::sort(pasteLayers.begin(), pasteLayers.end(), byRow);
    std::sort(silkLayers.begin(), silkLayers.end(), byRow);

    // Map copper: first = F.Cu (0), last = B.Cu (31), inner = In1.Cu through In28.Cu
    if (!copperLayers.empty()) {
        result[toLower(copperLayers.front().first)] = &kLayerMappings[0]; // F.Cu
        if (copperLayers.size() > 1) {
            result[toLower(copperLayers.back().first)] = &kLayerMappings[1]; // B.Cu
        }
        // Inner copper layers (KiCad IDs 1-30)
        // We use a static array for inner layer mappings
        static LayerMapping innerLayers[30];
        static bool innerInit = false;
        if (!innerInit) {
            for (int i = 0; i < 30; i++) {
                innerLayers[i].id = i + 1;
                static char names[30][16];
                snprintf(names[i], sizeof(names[i]), "In%d.Cu", i + 1);
                innerLayers[i].canonical = names[i];
                innerLayers[i].alias = nullptr;
            }
            innerInit = true;
        }
        for (size_t i = 1; i + 1 < copperLayers.size() && i <= 30; i++) {
            result[toLower(copperLayers[i].first)] = &innerLayers[i - 1];
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

KicadPcb transformToKicad(const OdbDesign& design) {
    KicadPcb pcb;

    // Build layer name mapping (handles both KiCad and non-KiCad ODB++ sources)
    auto layerMap = buildLayerMap(design.layers);

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

    // Helper: ODB layer name -> KiCad canonical name
    auto mapLayerName = [&layerMap](const std::string& odbName) -> std::string {
        auto it = layerMap.find(toLower(odbName));
        if (it != layerMap.end()) return it->second->canonical;
        return odbName;
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
                // Find the ODB++ copper layer name for this side
                std::string cuLayerName;
                for (auto& [name, m] : layerMap) {
                    if (m->id == (isBottom ? 31 : 0)) { cuLayerName = name; break; }
                }
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

                // Drill size for through-hole pads: look up from drill layer
                if (pad.type == "thru_hole") {
                    for (auto& [dlName, dlFeats] : design.layerFeatures) {
                        if (dlName.find("drill") == std::string::npos) continue;
                        for (auto& drillPad : dlFeats.pads) {
                            if (std::abs(drillPad.x - term.x) < 0.01 &&
                                std::abs(drillPad.y - term.y) < 0.01) {
                                if (drillPad.symIdx < (int)dlFeats.symbols.size()) {
                                    auto& ds = dlFeats.symbols[drillPad.symIdx];
                                    pad.drill = (ds.shape == OdbSymbol::ROUND) ? ds.diameter : ds.width;
                                }
                                break;
                            }
                        }
                        if (pad.drill > 0) break;
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

    // 4. Detect vias from drill layer features
    // Build a set of via positions from drill layers
    struct ViaInfo {
        double x, y;           // ODB++ coordinates
        double drillDiameter;  // from drill layer symbol
        double padSize;        // from copper layer symbol
        int netId;
    };
    std::vector<ViaInfo> viaInfos;

    for (auto& [layerNameLower, features] : design.layerFeatures) {
        // Find drill layers (name contains "drill")
        if (layerNameLower.find("drill") == std::string::npos) continue;

        for (auto& pad : features.pads) {
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
            vi.netId = 0;

            // Drill diameter from drill-layer symbol
            if (pad.symIdx < (int)features.symbols.size()) {
                auto& sym = features.symbols[pad.symIdx];
                vi.drillDiameter = (sym.shape == OdbSymbol::ROUND) ? sym.diameter : sym.width;
            } else {
                vi.drillDiameter = 0.3; // default
            }

            // Find pad size from copper layer (F.Cu) at same position
            vi.padSize = vi.drillDiameter + 0.3; // default annular ring
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
                                vi.padSize = cuSym.diameter;
                            } else {
                                vi.padSize = std::max(cuSym.width, cuSym.height);
                            }
                        }
                        break;
                    }
                }
            }

            viaInfos.push_back(vi);
        }
    }

    // Assign nets to vias from netlist points with 'v' flag
    for (auto& vi : viaInfos) {
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
        kv.x = toKicadX(vi.x);
        kv.y = toKicadY(vi.y);
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
        pni.x = toKicadX(vi.x);
        pni.y = toKicadY(vi.y);
        pni.netId = vi.netId;
        pni.throughHole = true; // vias span all copper layers
        padLookup.push_back(pni);
    }

    // Helper to find net for a point on a given layer
    auto findNetAtPoint = [&](double kx, double ky, const std::string& layer) -> int {
        int layerId = -1;
        for (auto& m : kLayerMappings) {
            if (m.canonical == layer) { layerId = m.id; break; }
        }
        bool isCopperLayer = (layerId >= 0 && layerId <= 31);
        if (!isCopperLayer) return 0;

        for (auto& pni : padLookup) {
            // Through-hole pads match on any copper layer
            bool layerMatch = pni.throughHole ||
                (layerId == 0 && !pni.throughHole) || // SMD on F.Cu
                (layerId == 31 && !pni.throughHole);   // SMD on B.Cu (if the footprint is on B.Cu)
            // Actually: SMD pad only matches on its own layer.
            // For simplicity, TH pads match any copper layer, SMD pads always match
            // since they were placed from the correct footprint layer.
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

            // Net assignment: check both endpoints against pad and via positions
            seg.netId = findNetAtPoint(seg.x1, seg.y1, kicadLayer);
            if (seg.netId == 0) {
                seg.netId = findNetAtPoint(seg.x2, seg.y2, kicadLayer);
            }

            pcb.segments.push_back(seg);
        }
    }

    // Propagate nets along connected traces (multi-hop)
    // Traces sharing an endpoint inherit each other's net
    bool changed = true;
    while (changed) {
        changed = false;
        for (size_t i = 0; i < pcb.segments.size(); i++) {
            if (pcb.segments[i].netId > 0) continue;
            for (size_t j = 0; j < pcb.segments.size(); j++) {
                if (i == j || pcb.segments[j].netId == 0) continue;
                if (pcb.segments[i].layer != pcb.segments[j].layer) continue;
                // Check if they share an endpoint
                auto touches = [](double ax, double ay, double bx, double by) {
                    return std::abs(ax - bx) < 0.02 && std::abs(ay - by) < 0.02;
                };
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
                rect.x1 = toKicadX(minX);
                rect.y1 = toKicadY(maxY);
                rect.x2 = toKicadX(maxX);
                rect.y2 = toKicadY(minY);
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
            gl.x1 = toKicadX(pts[i].x);
            gl.y1 = toKicadY(pts[i].y);
            gl.x2 = toKicadX(pts[i + 1].x);
            gl.y2 = toKicadY(pts[i + 1].y);
            gl.width = 0.05;
            gl.type = "default";
            gl.layer = "Edge.Cuts";
            pcb.grLines.push_back(gl);
        }
    };

    if (!design.profile.surfaces.empty()) {
        for (auto& surface : design.profile.surfaces) {
            for (auto& contour : surface.contours) {
                emitContourAsEdgeCuts(contour);
            }
        }
    }

    // 8. Edge.Cuts layer features (lines, arcs, circles from features file)
    {
        auto ecIt = design.layerFeatures.find("edge.cuts");
        if (ecIt != design.layerFeatures.end()) {
            auto& ecFeats = ecIt->second;
            for (auto& line : ecFeats.lines) {
                KicadGrLine gl;
                gl.x1 = toKicadX(line.x1);
                gl.y1 = toKicadY(line.y1);
                gl.x2 = toKicadX(line.x2);
                gl.y2 = toKicadY(line.y2);
                if (line.symIdx < (int)ecFeats.symbols.size()) {
                    auto& sym = ecFeats.symbols[line.symIdx];
                    gl.width = (sym.shape == OdbSymbol::ROUND) ? sym.diameter : sym.width;
                } else {
                    gl.width = 0.05;
                }
                gl.type = "default";
                gl.layer = "Edge.Cuts";
                pcb.grLines.push_back(gl);
            }
            for (auto& arc : ecFeats.arcs) {
                KicadGrArc ga;
                ga.xs = toKicadX(arc.xs);
                ga.ys = toKicadY(arc.ys);
                ga.xe = toKicadX(arc.xe);
                ga.ye = toKicadY(arc.ye);
                double xm_odb, ym_odb;
                computeArcMidpoint(arc.xs, arc.ys, arc.xe, arc.ye,
                                   arc.xc, arc.yc, arc.clockwise, xm_odb, ym_odb);
                ga.xm = toKicadX(xm_odb);
                ga.ym = toKicadY(ym_odb);
                if (arc.symIdx < (int)ecFeats.symbols.size()) {
                    auto& sym = ecFeats.symbols[arc.symIdx];
                    ga.width = (sym.shape == OdbSymbol::ROUND) ? sym.diameter : sym.width;
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
                    // For donut symbols, use the outer diameter to draw a circle
                    double radius = 0;
                    if (sym.name.find("donut_r") == 0) {
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
                        double cx = toKicadX(pad.x);
                        double cy = toKicadY(pad.y);
                        // First half: right -> top -> left
                        ga.xs = cx + radius;
                        ga.ys = cy;
                        ga.xm = cx;
                        ga.ym = cy - radius;
                        ga.xe = cx - radius;
                        ga.ye = cy;
                        ga.width = 0.05;
                        ga.layer = "Edge.Cuts";
                        pcb.grArcs.push_back(ga);
                        // Second half: left -> bottom -> right
                        ga.xs = cx - radius;
                        ga.ys = cy;
                        ga.xm = cx;
                        ga.ym = cy + radius;
                        ga.xe = cx + radius;
                        ga.ye = cy;
                        pcb.grArcs.push_back(ga);
                    }
                }
            }
        }
    }

    // 9. Copper fill zones from surface records on copper layers
    for (auto& [layerNameLower, features] : design.layerFeatures) {
        auto lmIt = layerMap.find(layerNameLower);
        if (lmIt == layerMap.end()) continue;
        auto* mapping = lmIt->second;
        if (mapping->id > 31) continue; // copper only
        std::string kicadLayer = mapping->canonical;

        for (auto& surface : features.surfaces) {
            KicadZone zone;
            zone.layer = kicadLayer;
            zone.netId = 0;
            zone.netName = "";

            // Collect all island (non-hole) contour points as outline
            // and all contours as filled polygons
            for (auto& contour : surface.contours) {
                std::vector<std::pair<double,double>> polyPts;
                for (size_t i = 1; i < contour.points.size(); i++) {
                    // Skip closing duplicate
                    if (i == contour.points.size() - 1 && contour.points.size() > 2 &&
                        std::abs(contour.points[i].x - contour.points[1].x) < 0.001 &&
                        std::abs(contour.points[i].y - contour.points[1].y) < 0.001)
                        continue;
                    polyPts.push_back({toKicadX(contour.points[i].x),
                                       toKicadY(contour.points[i].y)});
                }
                if (!contour.isHole && zone.outlinePoints.empty()) {
                    zone.outlinePoints = polyPts;
                }
                zone.filledPolygons.push_back(polyPts);
            }

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
    std::set<std::string> skipLayers = {"edge.cuts"}; // already handled
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
            gl.x1 = toKicadX(line.x1);
            gl.y1 = toKicadY(line.y1);
            gl.x2 = toKicadX(line.x2);
            gl.y2 = toKicadY(line.y2);
            if (line.symIdx < (int)features.symbols.size()) {
                auto& sym = features.symbols[line.symIdx];
                gl.width = (sym.shape == OdbSymbol::ROUND) ? sym.diameter : sym.width;
            } else {
                gl.width = 0.1;
            }
            gl.type = "solid";
            gl.layer = kicadLayer;
            pcb.grLines.push_back(gl);
        }

        for (auto& arc : features.arcs) {
            KicadGrArc ga;
            ga.xs = toKicadX(arc.xs);
            ga.ys = toKicadY(arc.ys);
            ga.xe = toKicadX(arc.xe);
            ga.ye = toKicadY(arc.ye);
            double xm_odb, ym_odb;
            computeArcMidpoint(arc.xs, arc.ys, arc.xe, arc.ye,
                               arc.xc, arc.yc, arc.clockwise, xm_odb, ym_odb);
            ga.xm = toKicadX(xm_odb);
            ga.ym = toKicadY(ym_odb);
            if (arc.symIdx < (int)features.symbols.size()) {
                auto& sym = features.symbols[arc.symIdx];
                ga.width = (sym.shape == OdbSymbol::ROUND) ? sym.diameter : sym.width;
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

    // Graphic arcs
    for (auto& ga : pcb.grArcs) {
        out << "\n  (gr_arc (start " << formatFloat(ga.xs) << " " << formatFloat(ga.ys)
            << ") (mid " << formatFloat(ga.xm) << " " << formatFloat(ga.ym)
            << ") (end " << formatFloat(ga.xe) << " " << formatFloat(ga.ye)
            << ") (stroke (width " << formatFloat(ga.width)
            << ") (type solid)) (layer \"" << ga.layer << "\"))\n";
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
