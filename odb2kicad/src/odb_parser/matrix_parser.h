#pragma once
#include "model/odb_design.h"
#include <string>
#include <vector>

namespace odb2kicad {

bool parseMatrix(const std::string& path, std::string& stepName, std::vector<OdbLayer>& layers);

} // namespace odb2kicad
