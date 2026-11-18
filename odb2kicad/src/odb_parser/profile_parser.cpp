#include "odb_parser/profile_parser.h"
#include "odb_parser/contour.h"
#include "util/odb_file.h"
#include "util/string_utils.h"
#include <fstream>
#include <sstream>

namespace odb2kicad {

bool parseProfile(const std::string& path, OdbProfile& profile) {
    auto opened = openOdbFile(path);  // the file, or its compressed twin <path>.z
    if (!opened) return false;
    std::istream& file = *opened;

    std::string line;
    OdbSurface* currentSurface = nullptr;
    OdbContour* currentContour = nullptr;

    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;

        if (startsWith(line, "UNITS=")) { profile.units = trim(line.substr(6)); continue; }
        if (startsWith(line, "F ")) continue;

        if (startsWith(line, "S ")) {
            // S polarity dcode
            profile.surfaces.emplace_back();
            currentSurface = &profile.surfaces.back();
            // Parse polarity
            auto parts = split(line, ' ');
            if (parts.size() >= 2) currentSurface->polarity = parts[1];
            continue;
        }

        // Contour records (OB / OS / OC / OE)
        if (parseContourRecord(line, currentSurface, currentContour, contourTolerance(profile.units))) continue;

        if (line == "SE") {
            currentSurface = nullptr;
            continue;
        }
    }
    return true;
}

} // namespace odb2kicad
