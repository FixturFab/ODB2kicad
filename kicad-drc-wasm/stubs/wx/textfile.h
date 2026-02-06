#pragma once
#include "string.h"
class wxTextFile {
public:
    wxTextFile() = default;
    wxTextFile(const wxString& filename) : m_name(filename) {}
    bool Open() { return false; }
    bool Open(const wxString& filename) { m_name = filename; return false; }
    void Close() {}
    bool IsOpened() const { return false; }
    size_t GetLineCount() const { return 0; }
    wxString GetLine(size_t) const { return wxString(); }
    wxString& GetLine(size_t) { static wxString s; return s; }
    wxString GetFirstLine() { return wxString(); }
    wxString GetNextLine() { return wxString(); }
    bool Eof() const { return true; }
    void AddLine(const wxString&) {}
    bool Write() { return false; }
    wxString GetName() const { return m_name; }
private:
    wxString m_name;
};
