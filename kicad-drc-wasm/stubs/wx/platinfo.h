#pragma once
#include "string.h"

// wxPlatformInfo - provides info about the current platform
class wxPlatformInfo {
public:
    static wxPlatformInfo& Get() { static wxPlatformInfo info; return info; }
    wxString GetOperatingSystemDescription() const { return wxString("WebAssembly"); }
    wxString GetDesktopEnvironment() const { return wxString("none"); }
    wxString GetBitnessName() const { return wxString("32 bit"); }
    wxString GetEndiannessName() const { return wxString("Little endian"); }
    wxString GetPortIdName() const { return wxString("wxWASM"); }
    int GetOSMajorVersion() const { return 1; }
    int GetOSMinorVersion() const { return 0; }
};

// Architecture info functions
inline wxString wxGetCpuArchitectureName() { return wxString("wasm32"); }
inline wxString wxGetNativeCpuArchitectureName() { return wxString("wasm32"); }

// wxVersionInfo - library version information
class wxVersionInfo
{
public:
    wxVersionInfo(const wxString& name = wxString(), int major = 0, int minor = 0, int micro = 0)
        : m_name(name), m_major(major), m_minor(minor), m_micro(micro) {}
    wxString GetVersionString() const {
        return wxString::Format(wxString("%d.%d.%d"), m_major, m_minor, m_micro);
    }
    wxString ToString() const { return GetVersionString(); }
    int GetMajor() const { return m_major; }
    int GetMinor() const { return m_minor; }
    int GetMicro() const { return m_micro; }
private:
    wxString m_name;
    int m_major, m_minor, m_micro;
};

inline wxVersionInfo wxGetLibraryVersionInfo() {
    return wxVersionInfo(wxString("wxWidgets-stub"), 3, 2, 0);
}

// wx version string
#define wxVERSION_NUM_DOT_STRING "3.2.0"

// Binary options defines (used in build_version.cpp)
// These are used as: aMsg << __WX_BO_UNICODE __WX_BO_STL; (no parens, string concat)
#define __WX_BO_UNICODE ""
#define __WX_BO_STL ""
#define __WX_BO_WXWIN_COMPAT_2_8 ""
#define __WX_BO_WXWIN_COMPAT_3_0 ""
