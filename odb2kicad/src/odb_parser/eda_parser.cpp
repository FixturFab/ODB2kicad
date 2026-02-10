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
            net.name = trim(line.substr(4));
            edaData.nets.push_back(net);
            currentPkg = nullptr;
            continue;
        }

        if (startsWith(line, "PKG ")) {
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

        // Skip SNT, FID, CT, OB, OS, OE, CE records for now
    }
    return true;
}

} // namespace odb2kicad
