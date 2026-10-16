#include "odb_parser/eda_parser.h"
#include "util/string_utils.h"
#include <fstream>
#include <sstream>

namespace odb2kicad {

bool parseEdaData(const std::string& path, OdbEdaData& edaData) {
    std::ifstream file(path);
    if (!file.is_open()) return false;

    std::string line;
    OdbEdaPackage* currentPkg = nullptr;
    int currentNet = -1;    // FID records belong to the last NET

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
            currentPkg = nullptr;
            continue;
        }

        // FID <type> <layer_idx> <feature_idx>: a feature of the current net
        if (startsWith(line, "FID ") && currentNet >= 0) {
            std::istringstream iss(line.substr(4));
            OdbEdaFid fid;
            fid.netIdx = currentNet;
            if (iss >> fid.type >> fid.layerIdx >> fid.featIdx) edaData.fids.push_back(fid);
            continue;
        }

        if (startsWith(line, "PKG ")) {
            currentNet = -1;
            // PKG name xmax ymin xmin ymax rotation;
            auto rest = line.substr(4);
            // Remove trailing semicolon
            if (!rest.empty() && rest.back() == ';') rest.pop_back();
            std::istringstream iss(rest);
            OdbEdaPackage pkg;
            double xmax, ymin, xmin, ymax, rot;
            iss >> pkg.name >> xmax >> ymin >> xmin >> ymax >> rot;
            edaData.packages.push_back(pkg);
            currentPkg = &edaData.packages.back();
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

        // Skip SNT (subnet headers; FID carries the mapping), CT, OB, OS, OE, CE records
    }
    return true;
}

} // namespace odb2kicad
