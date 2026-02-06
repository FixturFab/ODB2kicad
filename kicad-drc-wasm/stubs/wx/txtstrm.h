#pragma once

#include "stream.h"
#include "string.h"

class wxTextInputStream
{
public:
    wxTextInputStream(wxInputStream& stream) : m_stream(stream) {}
    wxString ReadLine() { return wxString(); }
    wxString ReadWord() { return wxString(); }
private:
    wxInputStream& m_stream;
};

class wxTextOutputStream
{
public:
    wxTextOutputStream(wxOutputStream& stream) : m_stream(stream) {}
    void WriteString(const wxString& s) { m_stream.Write(s.c_str(), s.length()); }
    wxTextOutputStream& operator<<(const wxString& s) { WriteString(s); return *this; }
private:
    wxOutputStream& m_stream;
};
