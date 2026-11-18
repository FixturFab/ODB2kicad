#pragma once
#include "model/odb_design.h"
#include <string>
#include <vector>

namespace odb2kicad {

// matrix/matrix: every STEP record and every LAYER row, in file order
bool parseMatrix(const std::string& path, std::vector<OdbMatrixStep>& steps, std::vector<OdbLayer>& layers);

// The same, with the first STEP record's name only
bool parseMatrix(const std::string& path, std::string& stepName, std::vector<OdbLayer>& layers);

} // namespace odb2kicad
