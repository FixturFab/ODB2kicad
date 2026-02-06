#pragma once
#include "string.h"
class wxCmdLineParser {
public:
    wxCmdLineParser() = default;
    wxCmdLineParser(int, char**) {}
    void SetDesc(const void*) {}
    bool Parse(bool = true) { return true; }
    bool Found(const wxString&) const { return false; }
    bool Found(const wxString&, wxString*) const { return false; }
    bool Found(const wxString&, long*) const { return false; }
    size_t GetParamCount() const { return 0; }
    wxString GetParam(size_t = 0) const { return wxString(); }
    void AddSwitch(const wxString&, const wxString& = wxString(), const wxString& = wxString(), int = 0) {}
    void AddOption(const wxString&, const wxString& = wxString(), const wxString& = wxString(), int = 0, int = 0) {}
    void AddParam(const wxString& = wxString(), int = 0, int = 0) {}
};
