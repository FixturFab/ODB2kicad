#pragma once

// Minimal wxWidgets defs.h stub for Emscripten/WASM build

#include <cstdint>
#include <cstddef>

// WXUNUSED macro - marks unused parameters
#define WXUNUSED(identifier) identifier

// Basic wx types
typedef int wxCoord;
typedef int wxWindowID;

// Constants
#define wxID_ANY (-1)
#define wxID_NONE (-3)
#define wxID_OK 5100
#define wxID_CANCEL 5101
#define wxID_YES 5103
#define wxID_NO 5104
#define wxID_HIGHEST 5999
#define wxID_OPEN 5000
#define wxID_CLOSE 5001
#define wxID_NEW 5002
#define wxID_SAVE 5003
#define wxID_SAVEAS 5004
#define wxID_EXIT 5006
#define wxID_UNDO 5007
#define wxID_REDO 5008
#define wxID_HELP 5009
#define wxID_PRINT 5010
#define wxID_ABOUT 5012
#define wxID_PREFERENCES 5013
#define wxID_CUT 5014
#define wxID_COPY 5015
#define wxID_PASTE 5016
#define wxID_DELETE 5017
#define wxID_FIND 5018
#define wxID_REPLACE 5019
#define wxID_SELECTALL 5020
#define wxID_APPLY 5021
#define wxID_ZOOM_IN 5022
#define wxID_ZOOM_OUT 5023
#define wxID_ZOOM_FIT 5024

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
enum wxStockCursor { wxCURSOR_NONE = 0, wxCURSOR_ARROW, wxCURSOR_HAND, wxCURSOR_CROSS, wxCURSOR_BULLSEYE, wxCURSOR_SIZING, wxCURSOR_IBEAM, wxCURSOR_MAX };

// Print orientation
enum wxPrintOrientation { wxPORTRAIT = 1, wxLANDSCAPE = 2 };

// Paper sizes
enum wxPaperSize {
    wxPAPER_NONE = 0,
    wxPAPER_LETTER,
    wxPAPER_LEGAL,
    wxPAPER_A4,
    wxPAPER_CSHEET,
    wxPAPER_DSHEET,
    wxPAPER_ESHEET,
    wxPAPER_TABLOID,
    wxPAPER_A3 = 8,
    wxPAPER_A5 = 11,
    wxPAPER_A2 = 66,
    wxPAPER_A1 = 67,
    wxPAPER_A0 = 68
};

// Menu/toolbar item kind
enum wxItemKind { wxITEM_SEPARATOR = -1, wxITEM_NORMAL = 0, wxITEM_CHECK, wxITEM_RADIO, wxITEM_DROPDOWN, wxITEM_MAX };

// Icon flags
#define wxICON_NONE         0x00000000
#define wxICON_INFORMATION  0x00000100
#define wxICON_WARNING      0x00000200
#define wxICON_ERROR        0x00000400
#define wxICON_QUESTION     0x00000800
#define wxICON_EXCLAMATION  wxICON_WARNING

// Window styles
#define wxTAB_TRAVERSAL     0x00080000
#define wxBORDER_NONE       0x00200000
#define wxBORDER_SIMPLE     0x00400000
#define wxWANTS_CHARS       0x00040000
#define wxFULL_REPAINT_ON_RESIZE 0x00010000
#define wxCLIP_CHILDREN     0x00400000
#define wxNO_BORDER         wxBORDER_NONE

// Key codes
enum wxKeyCode {
    WXK_BACK = 8, WXK_TAB = 9, WXK_RETURN = 13, WXK_ESCAPE = 27,
    WXK_SPACE = 32, WXK_DELETE = 127,
    WXK_START = 300, WXK_LBUTTON, WXK_RBUTTON, WXK_CANCEL, WXK_MBUTTON,
    WXK_CLEAR, WXK_SHIFT, WXK_ALT, WXK_CONTROL, WXK_MENU, WXK_PAUSE,
    WXK_CAPITAL, WXK_END, WXK_HOME, WXK_LEFT, WXK_UP, WXK_RIGHT, WXK_DOWN,
    WXK_SELECT, WXK_PRINT, WXK_EXECUTE, WXK_SNAPSHOT, WXK_INSERT, WXK_HELP,
    WXK_NUMPAD0, WXK_NUMPAD1, WXK_NUMPAD2, WXK_NUMPAD3, WXK_NUMPAD4,
    WXK_NUMPAD5, WXK_NUMPAD6, WXK_NUMPAD7, WXK_NUMPAD8, WXK_NUMPAD9,
    WXK_MULTIPLY, WXK_ADD, WXK_SEPARATOR, WXK_SUBTRACT, WXK_DECIMAL, WXK_DIVIDE,
    WXK_F1, WXK_F2, WXK_F3, WXK_F4, WXK_F5, WXK_F6, WXK_F7, WXK_F8,
    WXK_F9, WXK_F10, WXK_F11, WXK_F12, WXK_F13, WXK_F14, WXK_F15, WXK_F16,
    WXK_F17, WXK_F18, WXK_F19, WXK_F20, WXK_F21, WXK_F22, WXK_F23, WXK_F24,
    WXK_NUMLOCK, WXK_SCROLL, WXK_PAGEUP, WXK_PAGEDOWN,
    WXK_NUMPAD_SPACE, WXK_NUMPAD_TAB, WXK_NUMPAD_ENTER,
    WXK_NUMPAD_F1, WXK_NUMPAD_F2, WXK_NUMPAD_F3, WXK_NUMPAD_F4,
    WXK_NUMPAD_HOME, WXK_NUMPAD_LEFT, WXK_NUMPAD_UP, WXK_NUMPAD_RIGHT,
    WXK_NUMPAD_DOWN, WXK_NUMPAD_PAGEUP, WXK_NUMPAD_PAGEDOWN,
    WXK_NUMPAD_END, WXK_NUMPAD_BEGIN, WXK_NUMPAD_INSERT, WXK_NUMPAD_DELETE,
    WXK_NUMPAD_EQUAL, WXK_NUMPAD_MULTIPLY, WXK_NUMPAD_ADD,
    WXK_NUMPAD_SEPARATOR, WXK_NUMPAD_SUBTRACT, WXK_NUMPAD_DECIMAL,
    WXK_NUMPAD_DIVIDE,
    WXK_WINDOWS_LEFT, WXK_WINDOWS_RIGHT, WXK_WINDOWS_MENU,
    WXK_RAW_CONTROL,
    WXK_NONE = 0
};

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

// Variadic function macros (simplified stubs)
// Generate a no-op variadic template function so callers compile
#define WX_DEFINE_VARARG_FUNC_VOID(name, numfixed, fixed, impl, implutf8) \
    template<typename... Args> void name(Args&&...) {}
#define WX_DEFINE_VARARG_FUNC(rettype, name, numfixed, fixed, impl, implutf8) \
    template<typename... Args> rettype name(Args&&...) { return rettype(); }

// Forward declare wxString for wxFormatString
class wxString;

// wxFormatString stub
class wxFormatString {
public:
    wxFormatString(const char* s = "") {}
    wxFormatString(const wxString& s) {}
    operator const char*() const { return ""; }
};

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

// wxFprintf/wxPrintf: see wx/crt.h (avoids circular include with wxString)
