#pragma once
#include "window.h"

class wxPanel : public wxWindow
{
public:
    wxPanel() = default;
    wxPanel(wxWindow* parent, wxWindowID id = wxID_ANY,
            const wxPoint& pos = wxPoint(), const wxSize& size = wxSize(),
            long style = 0) {}
    virtual ~wxPanel() = default;
};
