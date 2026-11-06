#pragma once
#include "model/odb_design.h"
#include <string>

namespace odb2kicad {

bool parseOdbDirectory(const std::string& rootDir, OdbDesign& design);

// The same, with the reason a job cannot be read: "<code>: <message>", code one of
// no-input (not a folder), not-odb (no matrix/matrix), bad-matrix (no LAYER records), no-step, no-layers.
bool parseOdbDirectory(const std::string& rootDir, OdbDesign& design, std::string& error);

// True for an archive written by KiCad 9 or earlier, whose circle export has the donut_r skew
// that applyProducerQuirks undoes.
bool hasKicadDonutSkew(const OdbInfo& info);

// True when misc/info names KiCad as the producer (ODB_SOURCE or SAVE_APP)
bool isKicadProducer(const OdbInfo& info);

// Undo known exporter bugs that the archive's misc/info identifies. Called by parseOdbDirectory.
void applyProducerQuirks(OdbDesign& design);

// Scale every file to mm by its own UNITS line (else misc/info's, else INCH), symbols by their
// I/M override when they have one. Called by parseOdbDirectory; the writer then works in mm.
void normalizeUnits(OdbDesign& design);

} // namespace odb2kicad
