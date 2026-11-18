#pragma once
#include "model/odb_design.h"
#include <string>
#include <vector>

namespace odb2kicad {

bool parseOdbDirectory(const std::string& rootDir, OdbDesign& design);

// The same, with the reason a job cannot be read: "<code>: <message>", code one of
// no-input (not a folder), not-odb (no matrix/matrix), bad-matrix (no LAYER records), no-step, no-layers.
bool parseOdbDirectory(const std::string& rootDir, OdbDesign& design, std::string& error);

// A job's first step: the matrix's first STEP record (lowest COL) that has a folder under steps/,
// else the first folder by name. parseOdbDirectory reads that one.
//
// One named step (the matrix's name or the folder's, any case); "" is the first step.
bool parseOdbStep(const std::string& rootDir, const std::string& stepName, OdbDesign& design, std::string& error);

// Every step of the job, one OdbDesign each (the job-level files are read once and shared), in
// the order of listOdbSteps. A step that cannot be read (no layer data and no STEP-REPEAT blocks)
// is left out with a warning in the first design's report; the call fails only when no step can
// be read. A panel step's STEP-REPEAT blocks are in its stepHeader.repeats; convert() places them.
bool parseOdbJob(const std::string& rootDir, std::vector<OdbDesign>& designs, std::string& error);

// The names of the job's steps in conversion order: the matrix's STEP records by COL (those that
// have a folder), then the folders the matrix does not name.
std::vector<std::string> listOdbSteps(const std::string& rootDir);

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

// True when a record file is ODB++ XML (ODX): its first 200 bytes hold '<', '>', "xml" and "ODX".
// The reader takes the text form only and refuses an ODX matrix.
bool isOdxXml(const std::string& path);

// Job-level checks after misc/info and the matrix are read, into design.report: the ODB++
// version (a job older than 8 is read as 8, with a warning), a layer TYPE outside the standard
// thirteen (drawn as DOCUMENT, warning), a drill span that names no copper row (through,
// warning), and the matrix's rows with their gaps. Called by the parse functions.
void checkJob(OdbDesign& design);

// Draw every T / V record with its font (design.fonts): its strokes are appended to the layer's
// lines (OdbLine::textIdx names the record) with round symbols of the stroke width, so every
// layer treats text like any other line work. Expects mm; called by normalizeUnits. Texts whose
// font the job does not have, characters the font lacks and barcodes are reported.
void drawTexts(OdbDesign& design);

// Report lines for the DOCUMENT layers the writer leaves out because they hold more features
// than design.documentFeatureLimit (the writer applies the same rule when it maps layers).
void reportDocumentLayers(OdbDesign& design);

// Board thickness from the job, in mm, into design.boardThickness / thicknessSource (and one
// report line saying which path was taken):
//   "attrlist": the stackup the layer attrlists describe, when a DIELECTRIC matrix row carries
//               .layer_dielectric: those rows, plus the copper rows (.copper_weight in oz, 34.8 um
//               each, else their own .layer_dielectric) and the solder mask rows' .layer_dielectric;
//   "tools":    else the largest THICKNESS of a drill layer's tools file;
//   "default":  else nothing (the writer's 1.6 mm).
// Called by parseOdbDirectory after normalizeUnits.
void computeBoardThickness(OdbDesign& design);

// A layer's output transform from its attrlist (.out_x_scale / .out_y_scale, .out_mirror,
// .out_angle), applied to that layer's features when it is not the identity: scale about the
// step's zero, then mirror in X, then turn clockwise. Called by normalizeUnits, before the origin.
void applyLayerOutputTransforms(OdbDesign& design);

// Move the step by its stephdr origin (x' = x + X_ORIGIN, y' = y + Y_ORIGIN): profile, layer
// features, component placements and netlist points; package and symbol geometry is local and
// stays. Expects the header and the step in the same units (normalizeUnits calls it once both
// are mm). The origin is then zeroed and kept in appliedX / appliedY.
void applyStepOrigin(OdbDesign& design);

} // namespace odb2kicad
