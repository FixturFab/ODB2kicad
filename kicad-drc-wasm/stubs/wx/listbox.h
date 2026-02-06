#pragma once
#include "window.h"
class wxListBox : public wxWindow {
public:
    wxListBox() = default;
    int GetSelection() const { return -1; }
    void SetSelection(int) {}
    void Append(const wxString&) {}
    void Clear() {}
    int GetCount() const { return 0; }
    wxString GetString(int) const { return wxString(); }
};
