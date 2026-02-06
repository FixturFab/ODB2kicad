#pragma once
#include "string.h"
class wxPlatformInfo {
public:
    static wxPlatformInfo& Get() { static wxPlatformInfo info; return info; }
    wxString GetOperatingSystemDescription() const { return wxString("WebAssembly"); }
    int GetOSMajorVersion() const { return 1; }
    int GetOSMinorVersion() const { return 0; }
};
