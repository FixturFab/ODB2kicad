#pragma once
#include "window.h"
class wxScrolledWindow : public wxWindow {
public:
    wxScrolledWindow() = default;
    wxScrolledWindow(wxWindow*, wxWindowID = wxID_ANY, const wxPoint& = wxPoint(),
                     const wxSize& = wxSize(), long = 0) {}
    void SetScrollbars(int, int, int, int, int = 0, int = 0, bool = false) {}
    void Scroll(int, int) {}
    void GetViewStart(int*, int*) const {}
    void GetVirtualSize(int*, int*) const {}
    void SetScrollRate(int, int) {}
    void CalcScrolledPosition(int, int, int*, int*) const {}
    void CalcUnscrolledPosition(int, int, int*, int*) const {}
};
class wxScrolledCanvas : public wxScrolledWindow {};
