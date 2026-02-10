#pragma once
#include "model/odb_design.h"
#include <string>

namespace odb2kicad {

bool parseNetlist(const std::string& path, OdbNetlist& netlist);

} // namespace odb2kicad
