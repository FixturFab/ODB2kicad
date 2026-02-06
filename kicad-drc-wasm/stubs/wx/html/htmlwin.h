#pragma once
#include "../window.h"

#define wxHW_DEFAULT_STYLE 0
#define wxHW_SCROLLBAR_AUTO 0
#define wxHW_NO_SELECTION 0

class wxHtmlLinkInfo {
public:
    wxHtmlLinkInfo() = default;
    wxString GetHref() const { return wxString(); }
    wxString GetTarget() const { return wxString(); }
};

class wxHtmlLinkEvent : public wxCommandEvent {
public:
    wxHtmlLinkEvent() = default;
    wxHtmlLinkEvent(int id, const wxHtmlLinkInfo& info) {}
    const wxHtmlLinkInfo& GetLinkInfo() const { return info_; }
private:
    wxHtmlLinkInfo info_;
};

class wxHtmlCellEvent : public wxCommandEvent {
public:
    wxHtmlCellEvent() = default;
};

class wxHtmlWindow : public wxWindow {
public:
    wxHtmlWindow() = default;
    wxHtmlWindow(wxWindow*, wxWindowID = wxID_ANY, const wxPoint& = wxPoint(),
                 const wxSize& = wxSize(), long = 0) {}
    virtual bool SetPage(const wxString&) { return true; }
    bool AppendToPage(const wxString&) { return true; }
    wxString ToText() const { return wxString(); }
    void SetBorders(int) {}
};
