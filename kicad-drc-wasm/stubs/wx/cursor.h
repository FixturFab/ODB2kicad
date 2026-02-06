#pragma once

#include "gdicmn.h"
#include "defs.h"

// wxCursor is defined in gdicmn.h
// wxStockCursor is defined in defs.h

// wxCursorBundle stub (wx 3.3+ only, but define for safety)
class wxCursorBundle {
public:
    wxCursorBundle() = default;
};

inline const wxCursor wxNullCursor;
