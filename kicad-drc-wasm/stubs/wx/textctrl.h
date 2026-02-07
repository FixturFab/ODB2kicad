#pragma once
#include "window.h"
#include "textentry.h"
class wxTextCtrl : public wxWindow, public wxTextEntry {
public:
    wxTextCtrl() = default;
    wxTextCtrl(wxWindow*, wxWindowID, const wxString& = wxString(),
               const wxPoint& = wxPoint(), const wxSize& = wxSize(), long = 0) {}
    wxString GetValue() const override { return wxString(); }
    void SetValue(const wxString&) override {}
    void AppendText(const wxString&) override {}
    void Clear() {}
    bool IsEmpty() const { return true; }
    long GetLastPosition() const { return 0; }
    void SetInsertionPointEnd() override {}
    bool IsEditable() const override { return false; }
    void SetSelection(long from, long to) override {}
    void SetInsertionPoint(long pos) override {}
};
#define wxTE_MULTILINE 0
#define wxTE_READONLY 0
#define wxTE_PROCESS_ENTER 0
