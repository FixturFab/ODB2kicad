#pragma once
#include "dialog.h"

class wxMessageDialog : public wxDialog {
public:
    wxMessageDialog(wxWindow* parent = nullptr, const wxString& message = wxString(),
                    const wxString& caption = wxString(), long style = wxOK | wxICON_INFORMATION)
        : wxDialog() {}
    void SetExtendedMessage(const wxString&) {}
    int ShowModal() { return wxOK; }
};

class wxGenericMessageDialog : public wxMessageDialog {
public:
    wxGenericMessageDialog(wxWindow* parent = nullptr, const wxString& message = wxString(),
                           const wxString& caption = wxString(), long style = wxOK | wxICON_INFORMATION)
        : wxMessageDialog(parent, message, caption, style) {}
};

inline int wxMessageBox(const wxString&, const wxString& = wxString(), int = 0, wxWindow* = nullptr) { return wxOK; }
