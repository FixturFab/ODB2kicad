#pragma once
#include "model/odb_design.h"
#include <string>

namespace odb2kicad {

bool parseComponents(const std::string& path, OdbComponents& components);

} // namespace odb2kicad
