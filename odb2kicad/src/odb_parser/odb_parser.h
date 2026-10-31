#pragma once
#include "model/odb_design.h"
#include <string>

namespace odb2kicad {

bool parseOdbDirectory(const std::string& rootDir, OdbDesign& design);

// True for an archive written by KiCad 9 or earlier, whose circle export has the donut_r skew
// that applyProducerQuirks undoes.
bool hasKicadDonutSkew(const OdbInfo& info);

// Undo known exporter bugs that the archive's misc/info identifies. Called by parseOdbDirectory.
void applyProducerQuirks(OdbDesign& design);

} // namespace odb2kicad
