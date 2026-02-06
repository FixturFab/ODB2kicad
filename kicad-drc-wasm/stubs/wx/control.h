#pragma once

#include "window.h"

class wxControl : public wxWindow
{
public:
    wxControl() = default;
    wxControl(wxWindow* parent, wxWindowID id = wxID_ANY,
              const wxPoint& pos = wxPoint(), const wxSize& size = wxSize(),
              long style = 0, const wxString& name = wxString()) {}
    virtual ~wxControl() = default;

    void SetLabel(const wxString& label) override {}
    wxString GetLabel() const override { return wxString(); }
    wxString GetLabelText() const { return wxString(); }
};
