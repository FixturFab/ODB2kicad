#pragma once
#include "window.h"
class wxComboBox : public wxWindow {
public:
    wxComboBox() = default;
    int GetSelection() const { return -1; }
    void SetSelection(int) {}
    wxString GetValue() const { return wxString(); }
    void SetValue(const wxString&) {}
    void Append(const wxString&) {}
    void Clear() {}
    int GetCount() const { return 0; }
    wxString GetString(int) const { return wxString(); }
};
