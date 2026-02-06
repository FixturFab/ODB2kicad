#pragma once
#include "window.h"
class wxWebView : public wxWindow {
public:
    static wxWebView* New(wxWindow* = nullptr, wxWindowID = wxID_ANY, const wxString& = wxString(),
                          const wxPoint& = wxPoint(), const wxSize& = wxSize()) { return nullptr; }
};
class wxWebViewEvent : public wxEvent {};
