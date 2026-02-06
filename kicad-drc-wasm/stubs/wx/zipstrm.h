#pragma once
#include "stream.h"
#include "datetime.h"
class wxZipEntry {
public:
    wxString GetName() const { return wxString(); }
    wxString GetInternalName() const { return wxString(); }
    bool IsDir() const { return false; }
    size_t GetSize() const { return 0; }
    wxDateTime GetDateTime() const { return wxDateTime(); }
};
class wxZipInputStream : public wxInputStream {
public:
    wxZipInputStream(wxInputStream&) {}
    wxZipEntry* GetNextEntry() { return nullptr; }
};
class wxZipOutputStream : public wxOutputStream {
public:
    wxZipOutputStream(wxOutputStream&) {}
    bool PutNextEntry(const wxString&) { return false; }
    bool PutNextDirEntry(const wxString&) { return false; }
    bool CloseEntry() { return false; }
};
