#pragma once

// Minimal wxWidgets defs.h stub for Emscripten/WASM build

#include <cstdint>
#include <cstddef>

// Basic wx types
typedef int wxCoord;
typedef int wxWindowID;

// Constants
#define wxID_ANY (-1)
#define wxID_OK 5100
#define wxID_CANCEL 5101
#define wxID_YES 5103
#define wxID_NO 5104

// Direction flags
enum wxDirection { wxLEFT = 0x0010, wxRIGHT = 0x0020, wxUP = 0x0040, wxDOWN = 0x0080 };
enum { wxTOP = wxUP, wxBOTTOM = wxDOWN };
enum { wxHORIZONTAL = 0x0004, wxVERTICAL = 0x0008, wxBOTH = wxHORIZONTAL | wxVERTICAL };

// Alignment
enum { wxALIGN_NOT = 0, wxALIGN_CENTER = 1, wxALIGN_LEFT = 0, wxALIGN_RIGHT = 2 };

// Stretch
enum { wxSTRETCH_NOT = 0, wxSHRINK = 0x1000, wxGROW = 0x2000, wxEXPAND = wxGROW };

// Return values
enum { wxYES = 0x00000002, wxOK = 0x00000004, wxNO = 0x00000008, wxCANCEL = 0x00000010 };

// Stock cursor IDs
enum wxStockCursor { wxCURSOR_NONE = 0, wxCURSOR_ARROW, wxCURSOR_HAND, wxCURSOR_CROSS };

// Menu/toolbar item kind
enum wxItemKind { wxITEM_SEPARATOR = -1, wxITEM_NORMAL = 0, wxITEM_CHECK, wxITEM_RADIO, wxITEM_DROPDOWN, wxITEM_MAX };

// Icon flags
#define wxICON_NONE         0x00000000
#define wxICON_INFORMATION  0x00000100
#define wxICON_WARNING      0x00000200
#define wxICON_ERROR        0x00000400
#define wxICON_QUESTION     0x00000800

// Window styles
#define wxTAB_TRAVERSAL     0x00080000
#define wxBORDER_NONE       0x00200000
#define wxBORDER_SIMPLE     0x00400000
#define wxWANTS_CHARS       0x00040000
#define wxFULL_REPAINT_ON_RESIZE 0x00010000
#define wxCLIP_CHILDREN     0x00400000
#define wxNO_BORDER         wxBORDER_NONE

// Font encoding
enum wxFontEncoding {
    wxFONTENCODING_SYSTEM = -1,
    wxFONTENCODING_DEFAULT = 0,
    wxFONTENCODING_ISO8859_1 = 1,
    wxFONTENCODING_ISO8859_15 = 15,
    wxFONTENCODING_UTF8 = 100,
    wxFONTENCODING_CP1250 = 1250,
    wxFONTENCODING_CP1252 = 1252,
    wxFONTENCODING_CP437 = 437,
    wxFONTENCODING_UNICODE = 2000,
    wxFONTENCODING_MAX
};

// Deprecation macros
#define wxDEPRECATED(x) x
#define wxDEPRECATED_MSG(msg, x) x
#define WXDLLIMPEXP_CORE
#define WXDLLIMPEXP_BASE
#define WXDLLIMPEXP_ADV
#define WXDLLIMPEXP_FWD_CORE
#define WXDLLIMPEXP_FWD_BASE

// Misc
#define wxT(x) x
#define wxS(x) x
#define _T(x) x
#define _(s) wxString(s)
#define wxPOSIXFILENAME 1

// wxWidgets version
#define wxMAJOR_VERSION 3
#define wxMINOR_VERSION 2
#define wxRELEASE_NUMBER 0
#define wxCHECK_VERSION(major,minor,release) \
    ((wxMAJOR_VERSION > (major)) || \
    (wxMAJOR_VERSION == (major) && wxMINOR_VERSION > (minor)) || \
    (wxMAJOR_VERSION == (major) && wxMINOR_VERSION == (minor) && wxRELEASE_NUMBER >= (release)))

// Platform defines
#ifndef __WXGTK__
// Don't define __WXGTK__ for WASM - we define our own platform
#endif

// Override WXUSINGDLL - we're statically linking
#ifdef WXUSINGDLL
#undef WXUSINGDLL
#endif

// File offset
#ifndef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64
#endif

// Unused parameter
#define wxUnusedVar(x) (void)(x)

// wxNOT_FOUND
#define wxNOT_FOUND (-1)

// Character functions
#include <cctype>
inline int wxToupper(int c) { return toupper(c); }
inline int wxTolower(int c) { return tolower(c); }
inline bool wxIsalpha(int c) { return isalpha(c); }
inline bool wxIsalnum(int c) { return isalnum(c); }

// Callback types
typedef void (*wxObjectEventFunction)(class wxEvent&);

// Forward declarations
class wxString;
class wxWindow;
class wxEvent;
class wxCommandEvent;
class wxEvtHandler;
class wxApp;
class wxAppConsole;

// Include full wxString definition so headers that use wxString by value work
#include "string.h"
