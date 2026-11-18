#pragma once
#include <utility>
#include <vector>

namespace odb2kicad {

using KeyholeRing = std::vector<std::pair<double,double>>;

// Fold the holes of an island into its outline with zero-width keyhole cuts (KiCad's gr_poly has
// no holes). Holes go in one at a time, in order: each is wound against the outline (as it stands
// then) and spliced in at the closest pair of points (outline vertex, hole vertex), the earliest
// outline vertex and then the earliest hole vertex winning a tie. The cut repeats both vertices.
KeyholeRing keyhole(KeyholeRing outer, std::vector<KeyholeRing> holes);

// The same by trying every pair: O(outline x hole) per hole. keyhole() gives the same points
// exactly; this is kept as its reference (unit tests compare the two).
KeyholeRing keyholeReference(KeyholeRing outer, std::vector<KeyholeRing> holes);

} // namespace odb2kicad
