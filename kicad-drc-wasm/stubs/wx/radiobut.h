#pragma once
#include "window.h"
class wxRadioButton : public wxWindow {
public:
    wxRadioButton() = default;
    bool GetValue() const { return false; }
    void SetValue(bool) {}
};
class wxRadioBox : public wxWindow {
public:
    wxRadioBox() = default;
    int GetSelection() const { return 0; }
    void SetSelection(int) {}
    wxString GetString(int) const { return wxString(); }
    int GetCount() const { return 0; }
};
