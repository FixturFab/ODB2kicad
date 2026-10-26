#include "odb_parser/eda_parser.h"
#include "util/string_utils.h"
#include <cmath>
#include <fstream>
#include <sstream>

namespace odb2kicad {

// Points of an arc from the current point (xs,ys) to (xe,ye) around (xc,yc), start excluded,
// in steps of at most 15 degrees. A start == end arc is a full circle.
static void flattenArc(double xs, double ys, double xe, double ye, double xc, double yc, bool cw,
                       std::vector<std::pair<double,double>>& out) {
    const double kPi = 3.14159265358979323846;
    double r = std::hypot(xs - xc, ys - yc);
    double a0 = std::atan2(ys - yc, xs - xc), a1 = std::atan2(ye - yc, xe - xc);
    double sweep = a1 - a0;
    if (cw) { while (sweep >= 0) sweep -= 2 * kPi; }
    else    { while (sweep <= 0) sweep += 2 * kPi; }
    int n = std::max(1, (int)std::ceil(std::fabs(sweep) / (kPi / 12) - 1e-9));
    for (int i = 1; i < n; i++) {
        double a = a0 + sweep * i / n;
        out.push_back({xc + r * std::cos(a), yc + r * std::sin(a)});
    }
    out.push_back({xe, ye});
}

// One package outline record (RC, CR, SQ) as a ring; false for anything else
static bool simpleOutline(const std::string& line, std::vector<std::pair<double,double>>& ring) {
    std::istringstream iss(line);
    std::string rec;
    iss >> rec;
    if (rec == "RC") {            // RC <llx> <lly> <width> <height>
        double x, y, w, h;
        if (!(iss >> x >> y >> w >> h)) return false;
        ring = {{x, y}, {x + w, y}, {x + w, y + h}, {x, y + h}};
    } else if (rec == "SQ") {     // SQ <xc> <yc> <half side>
        double x, y, hs;
        if (!(iss >> x >> y >> hs)) return false;
        ring = {{x - hs, y - hs}, {x + hs, y - hs}, {x + hs, y + hs}, {x - hs, y + hs}};
    } else if (rec == "CR") {     // CR <xc> <yc> <radius>
        double x, y, r;
        if (!(iss >> x >> y >> r)) return false;
        ring.clear();
        flattenArc(x + r, y, x + r, y, x, y, false, ring);
        ring.pop_back();          // the closing point repeats the first
        ring.insert(ring.begin(), {x + r, y});
    } else {
        return false;
    }
    return true;
}

bool parseEdaData(const std::string& path, OdbEdaData& edaData) {
    std::ifstream file(path);
    if (!file.is_open()) return false;

    std::string line;
    OdbEdaPackage* currentPkg = nullptr;
    int currentNet = -1;    // FID records belong to the last NET
    OdbEdaFid currentSnt;   // ... and to the last SNT
    int sntCount = 0;
    // CT..CE contour of the current package outline: OB starts a polygon (I island / H hole),
    // OS/OC add segments and arcs, OE closes it
    bool inContour = false;
    bool contourIsland = false;
    std::vector<std::pair<double,double>> contour;

    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;

        if (startsWith(line, "HDR ")) continue;

        if (startsWith(line, "UNITS=")) {
            edaData.units = line.substr(6);
            continue;
        }

        if (startsWith(line, "LYR ")) {
            auto rest = line.substr(4);
            edaData.layers = split(rest, ' ');
            continue;
        }

        if (startsWith(line, "NET ")) {
            OdbEdaNet net;
            std::string netStr = trim(line.substr(4));
            // Strip trailing attribute suffix (e.g. ";;ID=4588" or ";attr=val")
            auto semi = netStr.find(';');
            if (semi != std::string::npos) {
                netStr = netStr.substr(0, semi);
            }
            net.name = trim(netStr);
            edaData.nets.push_back(net);
            currentNet = (int)edaData.nets.size() - 1;
            currentSnt = OdbEdaFid();
            currentPkg = nullptr;
            continue;
        }

        // FID <type> <layer_idx> <feature_idx>: a feature of the current net
        // SNT <type> [TOP: <side> <comp> <toeprint>]: a subnet of the current net
        if (startsWith(line, "SNT ") && currentNet >= 0) {
            std::istringstream iss(line.substr(4));
            currentSnt = OdbEdaFid();
            currentSnt.sntIdx = sntCount++;
            iss >> currentSnt.sntType;
            if (currentSnt.sntType == "TOP") iss >> currentSnt.sntSide >> currentSnt.sntComp >> currentSnt.sntToeprint;
            continue;
        }

        if (startsWith(line, "FID ") && currentNet >= 0) {
            std::istringstream iss(line.substr(4));
            OdbEdaFid fid = currentSnt;
            fid.netIdx = currentNet;
            if (iss >> fid.type >> fid.layerIdx >> fid.featIdx) edaData.fids.push_back(fid);
            continue;
        }

        if (startsWith(line, "PKG ")) {
            currentNet = -1;
            // PKG <name> <pitch> <xmin> <ymin> <xmax> <ymax>;<attributes>
            auto rest = line.substr(4);
            OdbEdaPackage pkg;
            std::istringstream iss(rest.substr(0, rest.find(';')));
            iss >> pkg.name;
            edaData.packages.push_back(pkg);
            currentPkg = &edaData.packages.back();
            inContour = false;
            contour.clear();
            continue;
        }

        if (startsWith(line, "PIN ") && currentPkg) {
            std::istringstream iss(line.substr(4));
            OdbEdaPin pin;
            iss >> pin.id >> pin.side >> pin.x >> pin.y >> pin.rotation
                >> pin.electrical >> pin.shape;
            currentPkg->pins.push_back(pin);
            continue;
        }

        // Package outline: only before the package's first PIN (later ones are pin outlines)
        if (currentPkg && currentPkg->pins.empty()) {
            std::vector<std::pair<double,double>> ring;
            if (simpleOutline(line, ring)) { currentPkg->outlines.push_back(ring); continue; }
            if (line == "CT") { inContour = true; continue; }
            if (line == "CE") { inContour = false; continue; }
            if (inContour) {
                std::istringstream iss(line);
                std::string rec;
                iss >> rec;
                if (rec == "OB") {
                    double x, y; std::string kind;
                    iss >> x >> y >> kind;
                    contour = {{x, y}};
                    contourIsland = kind != "H";
                } else if (rec == "OS" && !contour.empty()) {
                    double x, y;
                    if (iss >> x >> y) contour.push_back({x, y});
                } else if (rec == "OC" && !contour.empty()) {
                    double xe, ye, xc, yc; std::string cw;
                    if (iss >> xe >> ye >> xc >> yc >> cw)
                        flattenArc(contour.back().first, contour.back().second, xe, ye, xc, yc,
                                   cw == "Y", contour);
                } else if (rec == "OE") {
                    if (contour.size() > 1 && std::hypot(contour.front().first - contour.back().first,
                                                         contour.front().second - contour.back().second) < 1e-9)
                        contour.pop_back();
                    if (contourIsland && contour.size() >= 3) currentPkg->outlines.push_back(contour);
                    contour.clear();
                }
                continue;
            }
        }

        // Pin outlines (CT, OB, OS, OE, CE after a PIN) are not read
    }
    return true;
}

} // namespace odb2kicad
