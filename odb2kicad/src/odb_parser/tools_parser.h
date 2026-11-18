#pragma once
#include "model/odb_design.h"
#include <string>

namespace odb2kicad {

// A drill layer's tools file (layers/<drill>/tools): THICKNESS, USER_PARAMS and one TOOLS { }
// block per tool. Lenient: a missing file is false, an empty one is an empty table, unknown
// keys are skipped. Sizes stay as written (thousandths of the file's unit) until normalizeUnits.
bool parseDrillTools(const std::string& path, OdbDrillTools& tools);

} // namespace odb2kicad
