#pragma once
#include "window.h"
class wxTextCtrl : public wxWindow {
public:
    wxTextCtrl() = default;
    wxTextCtrl(wxWindow*, wxWindowID, const wxString& = wxString(),
               const wxPoint& = wxPoint(), const wxSize& = wxSize(), long = 0) {}
    wxString GetValue() const { return wxString(); }
    void SetValue(const wxString&) {}
    void AppendText(const wxString&) {}
    void Clear() {}
    bool IsEmpty() const { return true; }
    long GetLastPosition() const { return 0; }
    void SetInsertionPointEnd() {}
};
#define wxTE_MULTILINE 0
#define wxTE_READONLY 0
#define wxTE_PROCESS_ENTER 0
