#pragma once
#include "window.h"
class wxGauge : public wxWindow {
public:
    wxGauge() = default;
    wxGauge(wxWindow*, wxWindowID, int, const wxPoint& = wxPoint(),
            const wxSize& = wxSize(), long = 0) {}
    void SetRange(int) {}
    void SetValue(int) {}
    int GetValue() const { return 0; }
    int GetRange() const { return 100; }
    void Pulse() {}
};
