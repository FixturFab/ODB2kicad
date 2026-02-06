#pragma once
#include "window.h"
class wxSlider : public wxWindow {
public:
    wxSlider() = default;
    int GetValue() const { return 0; }
    void SetValue(int) {}
    void SetRange(int, int) {}
};
