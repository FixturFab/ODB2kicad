#pragma once
#include "model/kicad_pcb.h"
#include <string>

namespace odb2kicad {

// Where one copy of a board goes in a panel. KiCad frame (mm, y down): a point p of the board
// lands at at + R(angle) * (p - pivot), R turning `angle` degrees clockwise as seen on screen
// (the ODB++ STEP-REPEAT ANGLE).
struct PanelPlacement {
    double pivotX = 0, pivotY = 0;  // the board's datum
    double atX = 0, atY = 0;        // where the datum goes in the panel
    double angle = 0;
    int instance = 1;               // 1-based copy number: net prefix and footprint uuids
    std::string netPrefix;          // prepended to every net name of the copy ("Board_1-")
    std::string edgeLayer = "Edge.Cuts";  // the layer the board's Edge.Cuts items are drawn on
};

// Append a moved copy of `board` to `panel`: footprints (position and angle; pads keep their
// footprint-frame offsets, their board-relative angles turn), tracks, arcs, vias, zones and
// board graphics. The copy's nets are added to the panel's under netPrefix + name (net 0 stays
// unconnected), its footprints get uuids of their own, and the layers it uses that the panel
// lacks are added. An axis-aligned rectangle that is turned off the axes becomes a polygon.
void placeBoardInPanel(KicadPcb& panel, const KicadPcb& board, const PanelPlacement& at);

// True when the board draws anything on Edge.Cuts
bool hasBoardEdge(const KicadPcb& pcb);

} // namespace odb2kicad
