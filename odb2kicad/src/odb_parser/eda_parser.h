#pragma once
#include "model/odb_design.h"
#include <string>

namespace odb2kicad {

bool parseEdaData(const std::string& path, OdbEdaData& edaData);

} // namespace odb2kicad
