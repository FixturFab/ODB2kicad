#pragma once
#include "window.h"
class wxStaticBox : public wxWindow {
public:
    wxStaticBox() = default;
    wxStaticBox(wxWindow*, wxWindowID, const wxString&, const wxPoint& = wxPoint(),
                const wxSize& = wxSize(), long = 0) {}
};
