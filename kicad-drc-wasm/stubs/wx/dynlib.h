#pragma once
#include "string.h"

// Dynamic library load flags
#define wxDL_VERBATIM  0x00000001
#define wxDL_NOW       0x00000002
#define wxDL_GLOBAL    0x00000004
#define wxDL_LAZY      0x00000008
#define wxDL_DEFAULT   (wxDL_NOW)

typedef void* wxDllType;

class wxDynamicLibrary {
public:
    wxDynamicLibrary() : m_handle(nullptr) {}
    wxDynamicLibrary(const wxString&, int = 0) : m_handle(nullptr) {}
    bool Load(const wxString&, int = 0) { return false; }
    void Unload() { m_handle = nullptr; }
    bool IsLoaded() const { return m_handle != nullptr; }
    void* GetSymbol(const wxString&) const { return nullptr; }
    void* RawGetSymbol(const wxString&) const { return nullptr; }
    static wxString GetDllExt() { return wxString(".so"); }
    static wxString CanonicalizeName(const wxString& name) { return name; }
    wxDllType Detach() { wxDllType h = m_handle; m_handle = nullptr; return h; }
    void Attach(wxDllType handle) { m_handle = handle; }
private:
    wxDllType m_handle;
};
