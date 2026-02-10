#pragma once
#include "model/odb_design.h"
#include "model/kicad_pcb.h"
#include <string>
#include <ostream>

namespace odb2kicad {

// Transform ODB++ design to KiCad PCB model
KicadPcb transformToKicad(const OdbDesign& design);

// Write KiCad PCB to output stream
void writeKicadPcb(std::ostream& out, const KicadPcb& pcb);

} // namespace odb2kicad
