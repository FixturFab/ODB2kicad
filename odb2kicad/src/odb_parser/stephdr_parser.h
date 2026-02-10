#pragma once
#include "model/odb_design.h"
#include <string>

namespace odb2kicad {

bool parseStepHeader(const std::string& path, OdbStepHeader& header);

} // namespace odb2kicad
