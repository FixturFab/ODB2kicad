#pragma once
#include "model/odb_design.h"
#include <string>

namespace odb2kicad {

bool parseOdbDirectory(const std::string& rootDir, OdbDesign& design);

} // namespace odb2kicad
