#pragma once
#include <string>
#include <vector>

namespace odb2kicad {

std::string trim(const std::string& s);
std::vector<std::string> split(const std::string& s, char delim);
std::string toUpper(const std::string& s);
std::string toLower(const std::string& s);
bool startsWith(const std::string& s, const std::string& prefix);

// Read a decimal int the way std::stoi does (leading blanks, optional sign, digits; anything after
// the digits is ignored) but without throwing: false when there are no digits or the value does
// not fit an int. Used wherever a record may carry a non-numeric key ("ID=...", "UID=...").
bool parseInt(const std::string& s, int& out);

} // namespace odb2kicad
