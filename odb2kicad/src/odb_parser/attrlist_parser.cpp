#include "odb_parser/attrlist_parser.h"
#include "util/odb_file.h"
#include "util/string_utils.h"
#include <cstdlib>
#include <fstream>

namespace odb2kicad {

bool parseAttrList(const std::string& path, OdbAttrList& attrs) {
    auto opened = openOdbFile(path);  // the file, or its compressed twin <path>.z
    if (!opened) return false;
    std::istream& file = *opened;
    attrs.present = true;
    std::string line;
    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = trim(line.substr(0, eq));
        std::string val = trim(line.substr(eq + 1));
        if (key.empty()) continue;
        if (key == "UNITS") attrs.units = val;
        else attrs.values[key] = val;
    }
    return true;
}

bool OdbAttrList::has(const std::string& key) const {
    auto it = values.find(key);
    return it != values.end() && !it->second.empty();
}

std::string OdbAttrList::text(const std::string& key) const {
    auto it = values.find(key);
    return it == values.end() ? "" : it->second;
}

double OdbAttrList::number(const std::string& key, double fallback) const {
    auto it = values.find(key);
    if (it == values.end() || it->second.empty()) return fallback;
    char* end = nullptr;
    double v = std::strtod(it->second.c_str(), &end);
    return end == it->second.c_str() ? fallback : v;
}

bool OdbAttrList::flag(const std::string& key) const {
    std::string v = toLower(text(key));
    return v == "yes" || v == "y" || v == "true" || v == "1";
}

} // namespace odb2kicad
