#pragma once
#include "window.h"
class wxStaticText : public wxWindow {
public:
    wxStaticText() = default;
    wxStaticText(wxWindow*, wxWindowID, const wxString&, const wxPoint& = wxPoint(),
                 const wxSize& = wxSize(), long = 0) {}
};
