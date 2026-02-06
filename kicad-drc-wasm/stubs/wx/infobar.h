#pragma once
#include "window.h"
#include "button.h"
#include "sizer.h"
#include "timer.h"

class wxInfoBarGeneric : public wxWindow
{
public:
    wxInfoBarGeneric() = default;
    wxInfoBarGeneric(wxWindow* parent, wxWindowID id = wxID_ANY) : wxWindow(parent, id) {}
    virtual ~wxInfoBarGeneric() = default;

    virtual void ShowMessage(const wxString& msg, int flags = wxICON_INFORMATION) {}
    virtual void Dismiss() {}
    virtual void AddButton(wxWindowID, const wxString& = wxString()) {}
    virtual void RemoveButton(wxWindowID) {}
    virtual size_t GetButtonCount() const { return 0; }
    virtual bool HasButtonId(wxWindowID) const { return false; }
};

typedef wxInfoBarGeneric wxInfoBar;
