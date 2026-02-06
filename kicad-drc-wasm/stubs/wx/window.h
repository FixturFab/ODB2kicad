#pragma once

#include "defs.h"
#include "string.h"
#include "event.h"
#include "gdicmn.h"

class wxWindow : public wxEvtHandler
{
public:
    wxWindow() = default;
    wxWindow(wxWindow* parent, wxWindowID id = wxID_ANY) {}
    virtual ~wxWindow() = default;

    virtual bool Show(bool show = true) { return true; }
    virtual bool Enable(bool enable = true) { return true; }
    bool IsEnabled() const { return true; }
    bool IsShown() const { return false; }

    virtual void SetLabel(const wxString&) {}
    virtual wxString GetLabel() const { return wxString(); }
    virtual void SetName(const wxString&) {}
    virtual wxString GetName() const { return wxString(); }

    void SetId(wxWindowID id) { m_id = id; }
    wxWindowID GetId() const { return m_id; }

    virtual wxSize GetSize() const { return wxSize(0, 0); }
    virtual void SetSize(int w, int h) {}
    virtual void SetSize(const wxSize& s) {}
    virtual wxPoint GetPosition() const { return wxPoint(0, 0); }
    virtual wxSize GetClientSize() const { return wxSize(0, 0); }
    virtual wxSize GetMinSize() const { return wxSize(0, 0); }
    virtual wxSize GetMaxSize() const { return wxSize(0, 0); }
    virtual void SetMinSize(const wxSize&) {}
    virtual void SetMaxSize(const wxSize&) {}

    virtual void Refresh(bool eraseBackground = true, const wxRect* rect = nullptr) {}
    virtual void Update() {}
    virtual void Layout() {}

    wxWindow* GetParent() const { return nullptr; }
    void SetToolTip(const wxString&) {}
    virtual void DoSetToolTipText(const wxString&) {}
    void SetCursor(const wxCursor&) {}

    void Freeze() {}
    void Thaw() {}
    bool IsFrozen() const { return false; }

    int GetDPI() const { return 96; }
    double GetDPIScaleFactor() const { return 1.0; }
    double GetContentScaleFactor() const { return 1.0; }

    virtual void SetFocus() {}
    bool HasFocus() const { return false; }

    virtual wxSize DoGetBestSize() const { return wxSize(0, 0); }
    virtual wxSize DoGetBestClientSize() const { return wxSize(0, 0); }
    virtual bool TransferDataToWindow() { return true; }
    virtual bool TransferDataFromWindow() { return true; }
    virtual bool Validate() { return true; }

    virtual bool Destroy() { return true; }
    bool Close(bool force = false) { return true; }
    virtual void Raise() {}
    void Center(int direction = 0) {}
    void Centre(int direction = 0) {}
    wxSize ConvertDialogToPixels(const wxSize& sz) const { return sz; }
    wxPoint ConvertDialogToPixels(const wxPoint& pt) const { return pt; }
    void Disable() { Enable(false); }
    void SetForegroundColour(const wxColour&) {}
    void SetBackgroundColour(const wxColour&) {}

    void CallAfter(std::function<void()> fn) { fn(); }

    void SetSizer(class wxSizer*, bool deleteOld = true) {}
    class wxSizer* GetSizer() const { return nullptr; }

    static wxWindow* FindWindowById(long id, const wxWindow* parent = nullptr) { return nullptr; }
    wxEvtHandler* GetEventHandler() const { return nullptr; }
    void PushEventHandler(wxEvtHandler*) {}
    wxEvtHandler* PopEventHandler(bool deleteHandler = false) { return nullptr; }

    void SetClientSize(int w, int h) {}
    void SetClientSize(const wxSize& s) {}

    void ClientToScreen(int* x, int* y) const {}
    void ScreenToClient(int* x, int* y) const {}
    wxPoint ClientToScreen(const wxPoint& pt) const { return pt; }
    wxPoint ScreenToClient(const wxPoint& pt) const { return pt; }

private:
    wxWindowID m_id = wxID_ANY;
};

class wxControl : public wxWindow
{
public:
    wxControl() = default;
    wxControl(wxWindow* parent, wxWindowID id = wxID_ANY) : wxWindow(parent, id) {}
};

class wxStaticBitmap : public wxWindow
{
public:
    wxStaticBitmap() = default;
    wxStaticBitmap(wxWindow*, wxWindowID, const wxBitmap&, const wxPoint& = wxPoint(),
                   const wxSize& = wxSize(), long = 0) {}
    void SetBitmap(const wxBitmap&) {}
    wxBitmap GetBitmap() const { return wxBitmap(); }
};

#include "dc.h"
#include "bmpbndl.h"
#include "scrolwin.h"

extern const wxPoint wxDefaultPosition;
extern const wxSize wxDefaultSize;
// wxEmptyString is defined as a macro in string.h

// Window list
#include <list>
typedef std::list<wxWindow*> wxWindowList;

// Window style constants
#define wxDEFAULT_FRAME_STYLE 0x0001
#define wxRESIZE_BORDER 0x0040
#define wxFRAME_FLOAT_ON_PARENT 0x0008
#define wxFRAME_NO_TASKBAR 0x0002
#define wxFRAME_TOOL_WINDOW 0x0004
#define wxSTAY_ON_TOP 0x8000
#define wxSYSTEM_MENU 0x0800
#define wxMINIMIZE_BOX 0x0400
#define wxMAXIMIZE_BOX 0x0200
#define wxCLOSE_BOX 0x1000
#define wxCAPTION 0x2000
#ifndef wxCLIP_CHILDREN
#define wxCLIP_CHILDREN 0x4000
#endif
#define wxDEFAULT_DIALOG_STYLE (wxCAPTION | wxSYSTEM_MENU | wxCLOSE_BOX)

// Note: wxPanelNameStr is defined in wx/event.h as wxString
// wxDialogNameStr and wxFrameNameStr are defined here
inline const wxString wxDialogNameStr("dialog");
inline const wxString wxFrameNameStr("frame");
