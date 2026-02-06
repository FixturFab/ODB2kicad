#pragma once
#include "window.h"
class wxCheckBox : public wxWindow {
public:
    wxCheckBox() = default;
    bool GetValue() const { return false; }
    void SetValue(bool) {}
    bool IsChecked() const { return false; }
};
