#pragma once

#include "window.h"

class wxPopupWindow : public wxWindow
{
public:
    wxPopupWindow() = default;
    wxPopupWindow(wxWindow* parent, int flags = 0) {}
    virtual ~wxPopupWindow() = default;
    void Position(const wxPoint&, const wxSize&) {}
};

class wxPopupTransientWindow : public wxPopupWindow
{
public:
    wxPopupTransientWindow() = default;
    wxPopupTransientWindow(wxWindow* parent, int flags = 0) {}
    virtual ~wxPopupTransientWindow() = default;
    virtual void Popup(wxWindow* focus = nullptr) {}
    virtual void Dismiss() {}
    virtual bool ProcessLeftDown(wxMouseEvent&) { return false; }
};
