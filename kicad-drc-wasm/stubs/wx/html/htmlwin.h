#pragma once
#include "../window.h"

class wxHtmlWindow : public wxWindow {
public:
    wxHtmlWindow() = default;
    wxHtmlWindow(wxWindow*, wxWindowID = wxID_ANY, const wxPoint& = wxPoint(),
                 const wxSize& = wxSize(), long = 0) {}
    bool SetPage(const wxString&) { return true; }
    bool AppendToPage(const wxString&) { return true; }
    wxString ToText() const { return wxString(); }
    void SetBorders(int) {}
};
