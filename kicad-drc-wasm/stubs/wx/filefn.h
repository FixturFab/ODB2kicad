#pragma once
#include "filename.h"
#include "string.h"
#include <cstdio>

inline int wxRemove(const wxString& path) { return remove(path.c_str()); }
inline int wxRename(const wxString& oldpath, const wxString& newpath) { return rename(oldpath.c_str(), newpath.c_str()); }
inline FILE* wxFopen(const wxString& path, const wxString& mode) { return fopen(path.c_str(), mode.c_str()); }

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
