#pragma once
#include "string.h"
class wxDynamicLibrary {
public:
    wxDynamicLibrary() = default;
    wxDynamicLibrary(const wxString&, int = 0) {}
    bool Load(const wxString&, int = 0) { return false; }
    void Unload() {}
    bool IsLoaded() const { return false; }
    void* GetSymbol(const wxString&) const { return nullptr; }
    void* RawGetSymbol(const wxString&) const { return nullptr; }
    static wxString GetDllExt() { return wxString(".so"); }
    static wxString CanonicalizeName(const wxString& name) { return name; }
};
typedef void* wxDllType;
