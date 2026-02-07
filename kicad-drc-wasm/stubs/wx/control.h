#pragma once

#include "window.h"
#include "dc.h"

enum wxEllipsizeMode {
    wxELLIPSIZE_NONE = 0,
    wxELLIPSIZE_START,
    wxELLIPSIZE_MIDDLE,
    wxELLIPSIZE_END
};

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

    static wxString Ellipsize(const wxString& label, const wxDC& dc,
                              wxEllipsizeMode mode, int maxWidth, int flags = 0) {
        // Stub: just truncate if too long
        return label;
    }
};
