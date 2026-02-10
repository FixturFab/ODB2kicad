#pragma once
#include "model/odb_design.h"
#include <string>

namespace odb2kicad {

bool parseInfo(const std::string& path, OdbInfo& info);

} // namespace odb2kicad
