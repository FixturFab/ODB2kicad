#pragma once
#include "dialog.h"
class wxProgressDialog : public wxDialog {
public:
    wxProgressDialog(const wxString&, const wxString&, int = 100, wxWindow* = nullptr, int = 0) {}
    bool Update(int, const wxString& = wxString()) { return true; }
    bool Pulse(const wxString& = wxString()) { return true; }
    void Resume() {}
    bool WasCancelled() const { return false; }
};
