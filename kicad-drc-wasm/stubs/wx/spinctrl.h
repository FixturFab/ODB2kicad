#pragma once
#include "window.h"
class wxSpinCtrl : public wxWindow {
public:
    wxSpinCtrl() = default;
    int GetValue() const { return 0; }
    void SetValue(int) {}
    void SetRange(int, int) {}
};
class wxSpinCtrlDouble : public wxWindow {
public:
    wxSpinCtrlDouble() = default;
    double GetValue() const { return 0; }
    void SetValue(double) {}
    void SetRange(double, double) {}
    void SetIncrement(double) {}
    void SetDigits(int) {}
};
