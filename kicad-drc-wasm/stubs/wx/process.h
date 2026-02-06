#pragma once
#include "event.h"
class wxProcess : public wxEvtHandler {
public:
    wxProcess(wxEvtHandler* = nullptr, int = wxID_ANY) {}
    virtual ~wxProcess() = default;
    static bool Exists(int) { return false; }
    static int Execute(const wxString&, int = 0) { return -1; }
    void Redirect() {}
    wxInputStream* GetInputStream() { return nullptr; }
    wxInputStream* GetErrorStream() { return nullptr; }
    wxOutputStream* GetOutputStream() { return nullptr; }
};
inline long wxExecute(const wxString&, int = 0, wxProcess* = nullptr) { return -1; }
inline long wxExecute(const wxString&, wxArrayString&, int = 0) { return -1; }
inline long wxExecute(const wxString&, wxArrayString&, wxArrayString&, int = 0) { return -1; }
