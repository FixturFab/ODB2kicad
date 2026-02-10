#pragma once
#include "model/odb_design.h"
#include <string>

namespace odb2kicad {

bool parseFeatures(const std::string& path, OdbFeatures& features);

} // namespace odb2kicad
