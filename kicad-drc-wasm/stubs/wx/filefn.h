#pragma once
#include "filename.h"
#include "string.h"

// System info functions
inline wxString wxGetUserId() { return wxString("wasm_user"); }
inline wxString wxGetHostName() { return wxString("wasm_host"); }
inline wxString wxGetUserName() { return wxString("wasm_user"); }
inline wxString wxGetFullHostName() { return wxString("wasm_host"); }
inline wxString wxGetOsDescription() { return wxString("WebAssembly"); }

class wxPathList : public wxArrayString {
public:
    void AddEnvList(const wxString&) {}
    void Add(const wxString& path) { wxArrayString::Add(path); }
    wxString FindValidPath(const wxString& file) const { return wxString(); }
    wxString FindAbsoluteValidPath(const wxString& file) const { return wxString(); }
    bool EnsureFileAccessible(const wxString& file) { return true; }
};
