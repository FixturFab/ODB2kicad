#pragma once
#include <string>
#include <vector>

namespace odb2kicad {

std::string trim(const std::string& s);
std::vector<std::string> split(const std::string& s, char delim);
std::string toUpper(const std::string& s);
std::string toLower(const std::string& s);
bool startsWith(const std::string& s, const std::string& prefix);

} // namespace odb2kicad
