#pragma once
#include "model/odb_design.h"
#include <string>

namespace odb2kicad {

// An attrlist file (job, step or layer): "key = value" lines, every key kept. Lenient by
// construction: a missing file is false, an empty one (KiCad writes placeholders) an empty list,
// lines without '=' are skipped.
bool parseAttrList(const std::string& path, OdbAttrList& attrs);

} // namespace odb2kicad
