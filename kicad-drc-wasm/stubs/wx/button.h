#pragma once
#include "window.h"
class wxButton : public wxWindow {
public:
    wxButton() = default;
    wxButton(wxWindow*, wxWindowID, const wxString& = wxString(), const wxPoint& = wxPoint(),
             const wxSize& = wxSize(), long = 0) {}
};
class wxBitmapButton : public wxButton {};
class wxToggleButton : public wxButton {
public:
    bool GetValue() const { return false; }
    void SetValue(bool) {}
};
