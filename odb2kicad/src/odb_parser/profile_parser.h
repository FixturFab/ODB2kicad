#pragma once
#include "model/odb_design.h"
#include <string>

namespace odb2kicad {

bool parseProfile(const std::string& path, OdbProfile& profile);

} // namespace odb2kicad
