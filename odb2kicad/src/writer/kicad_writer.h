#pragma once
#include "model/odb_design.h"
#include "model/kicad_pcb.h"
#include <string>
#include <ostream>
#include <vector>

namespace odb2kicad {

// Transform ODB++ design to KiCad PCB model. With `report`, one "info:" line per matrix row
// saying which KiCad layer it is drawn on (or why it is not) is appended to it.
KicadPcb transformToKicad(const OdbDesign& design, std::vector<std::string>* report = nullptr);

// Write KiCad PCB to output stream
void writeKicadPcb(std::ostream& out, const KicadPcb& pcb);

} // namespace odb2kicad
