#pragma once

#include "window.h"
#include "menu.h"
#include "statusbr.h"

class wxStatusBar;

#define wxDEFAULT_FRAME_STYLE (0x00000020L | 0x00000040L | 0x00000200L | 0x00000400L | 0x00000800L | 0x00001000L)

class wxTopLevelWindow : public wxWindow
{
public:
    wxTopLevelWindow() = default;
    virtual ~wxTopLevelWindow() = default;
    void Maximize(bool = true) {}
    void Iconize(bool = true) {}
    bool IsMaximized() const { return false; }
    bool IsIconized() const { return false; }
    bool IsFullScreen() const { return false; }
    void SetTitle(const wxString&) {}
    wxString GetTitle() const { return wxString(); }
    void Centre(int = wxBOTH) {}
    void Center(int d = wxBOTH) { Centre(d); }
};

class wxFrame : public wxTopLevelWindow
{
public:
    wxFrame() = default;
    wxFrame(wxWindow* parent, wxWindowID id, const wxString& title,
            const wxPoint& pos = wxPoint(), const wxSize& size = wxSize(),
            long style = wxDEFAULT_FRAME_STYLE, const wxString& name = wxString()) {}
    virtual ~wxFrame() = default;

    bool ProcessEvent(wxEvent& event) override { return wxWindow::ProcessEvent(event); }

    void SetMenuBar(wxMenuBar*) {}
    wxMenuBar* GetMenuBar() const { return nullptr; }
    void SetStatusBar(wxStatusBar*) {}
    wxStatusBar* GetStatusBar() const { return nullptr; }
    void SetToolBar(void*) {}
    wxStatusBar* CreateStatusBar(int = 1) { return nullptr; }
    void SetStatusText(const wxString&, int = 0) {}
    void SendSizeEvent(int = 0) {}
};
