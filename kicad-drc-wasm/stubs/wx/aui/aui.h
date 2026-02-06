#pragma once
#include "../window.h"
#include "../frame.h"
#include "../checkbox.h"
#include "../choice.h"
#include "auibar.h"

class wxAuiPaneInfo {
public:
    // Internal flags enum for SetFlag
    enum wxAuiPaneInfoFlags {
        optionToolbar = 1 << 0,
    };

    // Public data members accessed by aui_settings.cpp serialization
    wxString     name;
    wxString     caption;
    int          state = 0;
    unsigned int dock_direction = 0;
    int          dock_layer = 0;
    int          dock_row = 0;
    int          dock_pos = 0;
    int          dock_proportion = 0;
    wxSize       best_size;
    wxSize       min_size;
    wxSize       max_size;
    wxPoint      floating_pos;
    wxSize       floating_size;
    wxRect       rect;

    wxAuiPaneInfo& Name(const wxString& n) { name = n; return *this; }
    wxAuiPaneInfo& Caption(const wxString& c) { caption = c; return *this; }
    wxAuiPaneInfo& Left() { return *this; }
    wxAuiPaneInfo& Right() { return *this; }
    wxAuiPaneInfo& Top() { return *this; }
    wxAuiPaneInfo& Bottom() { return *this; }
    wxAuiPaneInfo& Center() { return *this; }
    wxAuiPaneInfo& BestSize(int, int) { return *this; }
    wxAuiPaneInfo& BestSize(const wxSize&) { return *this; }
    wxAuiPaneInfo& MinSize(int, int) { return *this; }
    wxAuiPaneInfo& MinSize(const wxSize&) { return *this; }
    wxAuiPaneInfo& MaxSize(int, int) { return *this; }
    wxAuiPaneInfo& MaxSize(const wxSize&) { return *this; }
    wxAuiPaneInfo& Layer(int) { return *this; }
    wxAuiPaneInfo& Row(int) { return *this; }
    wxAuiPaneInfo& Position(int) { return *this; }
    wxAuiPaneInfo& CloseButton(bool = true) { return *this; }
    wxAuiPaneInfo& MaximizeButton(bool = true) { return *this; }
    wxAuiPaneInfo& MinimizeButton(bool = true) { return *this; }
    wxAuiPaneInfo& PinButton(bool = true) { return *this; }
    wxAuiPaneInfo& Dock() { return *this; }
    wxAuiPaneInfo& Float() { return *this; }
    wxAuiPaneInfo& Hide() { return *this; }
    wxAuiPaneInfo& Show(bool = true) { return *this; }
    wxAuiPaneInfo& CaptionVisible(bool = true) { return *this; }
    wxAuiPaneInfo& Resizable(bool = true) { return *this; }
    wxAuiPaneInfo& Fixed() { return *this; }
    wxAuiPaneInfo& Movable(bool = true) { return *this; }
    wxAuiPaneInfo& Dockable(bool = true) { return *this; }
    wxAuiPaneInfo& Floatable(bool = true) { return *this; }
    wxAuiPaneInfo& Gripper(bool = true) { return *this; }
    wxAuiPaneInfo& PaneBorder(bool = true) { return *this; }
    wxAuiPaneInfo& TopDockable(bool = true) { return *this; }
    wxAuiPaneInfo& BottomDockable(bool = true) { return *this; }
    wxAuiPaneInfo& LeftDockable(bool = true) { return *this; }
    wxAuiPaneInfo& RightDockable(bool = true) { return *this; }
    wxAuiPaneInfo& DockFixed(bool = true) { return *this; }
    wxAuiPaneInfo& ToolbarPane() { return *this; }
    wxAuiPaneInfo& Defaults() { return *this; }
    wxAuiPaneInfo& SetFlag(int, bool) { return *this; }
    bool IsShown() const { return false; }
};

class wxAuiManager {
public:
    wxAuiManager(wxWindow* = nullptr, unsigned int = 0) {}
    void SetManagedWindow(wxWindow*) {}
    void Update() {}
    void UnInit() {}
    bool AddPane(wxWindow*, int = 0) { return true; }
    bool AddPane(wxWindow*, const wxString&) { return true; }
    void DetachPane(wxWindow*) {}
    wxAuiPaneInfo& GetPane(const wxString&) { static wxAuiPaneInfo dummy; return dummy; }
    wxAuiPaneInfo& GetPane(wxWindow*) { static wxAuiPaneInfo dummy; return dummy; }
};

class wxAuiNotebook : public wxWindow {
public:
    wxAuiNotebook() = default;
    wxAuiNotebook(wxWindow*, wxWindowID = wxID_ANY, const wxPoint& = wxPoint(),
                  const wxSize& = wxSize(), long = 0) {}
    bool AddPage(wxWindow*, const wxString&, bool = false) { return true; }
    int GetSelection() const { return 0; }
    void SetSelection(size_t) {}
    size_t GetPageCount() const { return 0; }
    wxWindow* GetPage(size_t) const { return nullptr; }
    int GetPageIndex(wxWindow*) const { return -1; }
    bool DeletePage(size_t) { return true; }
    bool RemovePage(size_t) { return true; }
    void SetPageText(size_t, const wxString&) {}
    wxString GetPageText(size_t) const { return wxString(); }
};

// wxAuiToolBarItem and wxAuiToolBarEvent defined in auibar.h

#ifndef wxAUI_TB_DEFAULT_STYLE
#define wxAUI_TB_DEFAULT_STYLE 0
#define wxAUI_TB_TEXT 0x0001
#define wxAUI_TB_HORZ_LAYOUT 0x0002
#define wxAUI_TB_VERTICAL 0x0004
#define wxAUI_TB_OVERFLOW 0x0008
#define wxAUI_TB_PLAIN_BACKGROUND 0x0010

class wxAuiToolBar : public wxWindow
{
public:
    wxAuiToolBar() = default;
    wxAuiToolBar(wxWindow* parent, wxWindowID id = wxID_ANY,
                 const wxPoint& pos = wxPoint(), const wxSize& size = wxSize(),
                 long style = wxAUI_TB_DEFAULT_STYLE) {}
    void AddTool(int, const wxString&, const wxBitmapBundle&, const wxString& = wxString()) {}
    void AddSeparator() {}
    void Realize() {}
    void SetToolBitmap(int, const wxBitmapBundle&) {}
    virtual void DoSetToolTipText(const wxString&) {}
    virtual void OnCustomRender(wxDC&, const wxAuiToolBarItem&, const wxRect&) {}
};

#endif // wxAUI_TB_DEFAULT_STYLE

typedef int wxAuiManagerEvent;
typedef int wxAuiNotebookEvent;
