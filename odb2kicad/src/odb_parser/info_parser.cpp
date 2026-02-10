#include "odb_parser/info_parser.h"
#include "util/string_utils.h"
#include <fstream>

namespace odb2kicad {

bool parseInfo(const std::string& path, OdbInfo& info) {
    std::ifstream file(path);
    if (!file.is_open()) return false;

    std::string line;
    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;

        auto eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);

        if (key == "UNITS") info.units = val;
        else if (key == "JOB_NAME") info.jobName = val;
        else if (key == "ODB_SOURCE") info.odbSource = val;
    }
    return true;
}

} // namespace odb2kicad
