#pragma once
#include "window.h"
class wxStatusBar : public wxWindow {
public:
    wxStatusBar() = default;
    void SetFieldsCount(int) {}
    void SetStatusText(const wxString&, int = 0) {}
    wxString GetStatusText(int = 0) const { return wxString(); }
    void SetStatusWidths(int, const int*) {}
};
