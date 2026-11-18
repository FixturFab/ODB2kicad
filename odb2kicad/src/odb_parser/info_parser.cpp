#include "odb_parser/info_parser.h"
#include "util/odb_file.h"
#include "util/string_utils.h"
#include <cstdlib>
#include <fstream>

namespace odb2kicad {

bool parseInfo(const std::string& path, OdbInfo& info) {
    auto opened = openOdbFile(path);  // the file, or its compressed twin <path>.z
    if (!opened) return false;
    std::istream& file = *opened;

    std::string line;
    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;

        auto eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = trim(line.substr(0, eq));
        std::string val = trim(line.substr(eq + 1));
        info.values[key] = val;

        if (key == "UNITS") info.units = val;
        else if (key == "JOB_NAME") info.jobName = val;
        else if (key == "ODB_SOURCE") info.odbSource = val;
        else if (key == "SAVE_APP") info.saveApp = val;
        else if (key == "ODB_VERSION_MAJOR") { if (!val.empty()) info.versionMajor = std::atoi(val.c_str()); }
        else if (key == "ODB_VERSION_MINOR") { if (!val.empty()) info.versionMinor = std::atoi(val.c_str()); }
        else if (key == "CREATION_DATE") info.creationDate = val;
        else if (key == "SAVE_DATE") info.saveDate = val;
    }
    return true;
}

} // namespace odb2kicad
