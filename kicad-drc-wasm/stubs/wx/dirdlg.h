#pragma once
#include "dialog.h"
class wxDirDialog : public wxDialog {
public:
    wxDirDialog(wxWindow* = nullptr, const wxString& = wxString(), const wxString& = wxString(), long = 0) {}
    wxString GetPath() const { return wxString(); }
};
