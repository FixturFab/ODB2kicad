#include "odb_parser/profile_parser.h"
#include "util/string_utils.h"
#include <fstream>
#include <sstream>

namespace odb2kicad {

bool parseProfile(const std::string& path, OdbProfile& profile) {
    std::ifstream file(path);
    if (!file.is_open()) return false;

    std::string line;
    OdbSurface* currentSurface = nullptr;
    OdbContour* currentContour = nullptr;

    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;

        if (startsWith(line, "UNITS=") || startsWith(line, "F ")) continue;

        if (startsWith(line, "S ")) {
            // S polarity dcode
            profile.surfaces.emplace_back();
            currentSurface = &profile.surfaces.back();
            // Parse polarity
            auto parts = split(line, ' ');
            if (parts.size() >= 2) currentSurface->polarity = parts[1];
            continue;
        }

        if (startsWith(line, "OB ")) {
            // OB x y direction
            if (!currentSurface) continue;
            currentSurface->contours.emplace_back();
            currentContour = &currentSurface->contours.back();

            std::istringstream iss(line.substr(3));
            double x, y;
            std::string dir;
            iss >> x >> y >> dir;
            currentContour->isHole = (dir == "H");
            currentContour->points.push_back({x, y});
            continue;
        }

        if (startsWith(line, "OS ")) {
            // OS x y
            if (!currentContour) continue;
            std::istringstream iss(line.substr(3));
            double x, y;
            iss >> x >> y;
            currentContour->points.push_back({x, y});
            continue;
        }

        if (line == "OE") {
            currentContour = nullptr;
            continue;
        }

        if (line == "SE") {
            currentSurface = nullptr;
            continue;
        }
    }
    return true;
}

} // namespace odb2kicad
