#pragma once

#include "window.h"

class wxDialog : public wxWindow
{
public:
    wxDialog() = default;
    wxDialog(wxWindow* parent, wxWindowID id, const wxString& title,
             const wxPoint& pos = wxPoint(), const wxSize& size = wxSize(),
             long style = 0, const wxString& name = wxString()) {}
    virtual ~wxDialog() = default;

    virtual int ShowModal() { return wxID_CANCEL; }
    virtual void EndModal(int retCode) {}
    bool IsModal() const { return false; }
    void SetReturnCode(int code) {}
    int GetReturnCode() const { return wxID_CANCEL; }
};

#define wxDEFAULT_DIALOG_STYLE 0
