#pragma once
#include "window.h"
class wxSplitterWindow : public wxWindow {
public:
    wxSplitterWindow() = default;
    wxSplitterWindow(wxWindow*, wxWindowID = wxID_ANY, const wxPoint& = wxPoint(),
                     const wxSize& = wxSize(), long = 0) {}
    bool SplitHorizontally(wxWindow*, wxWindow*, int = 0) { return true; }
    bool SplitVertically(wxWindow*, wxWindow*, int = 0) { return true; }
    void Unsplit(wxWindow* = nullptr) {}
    void SetSashGravity(double) {}
    void SetMinimumPaneSize(int) {}
    int GetSashPosition() const { return 0; }
    void SetSashPosition(int) {}
};
