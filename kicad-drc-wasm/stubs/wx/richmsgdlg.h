#pragma once
#include "msgdlg.h"

// ButtonLabel stub
class ButtonLabel {
public:
    ButtonLabel(const wxString& s = wxString()) {}
    ButtonLabel(const char* s) {}
};

class wxRichMessageDialog : public wxMessageDialog {
public:
    wxRichMessageDialog(wxWindow* parent = nullptr, const wxString& message = wxString(),
                        const wxString& caption = wxString(), long style = wxOK | wxICON_INFORMATION)
        : wxMessageDialog(parent, message, caption, style) {}
    void ShowDetailedText(const wxString&) {}
    void ShowCheckBox(const wxString&, bool = false) {}
    bool IsCheckBoxChecked() const { return false; }
    virtual bool SetOKCancelLabels(const ButtonLabel&, const ButtonLabel&) { return true; }
    virtual bool SetYesNoCancelLabels(const ButtonLabel&, const ButtonLabel&, const ButtonLabel&) { return true; }
};

class wxGenericRichMessageDialog : public wxRichMessageDialog {
public:
    wxGenericRichMessageDialog(wxWindow* parent = nullptr, const wxString& message = wxString(),
                               const wxString& caption = wxString(), long style = wxOK | wxICON_INFORMATION)
        : wxRichMessageDialog(parent, message, caption, style) {}
};
