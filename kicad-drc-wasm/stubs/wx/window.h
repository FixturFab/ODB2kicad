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
    void SetCursor(const wxCursor&) {}

    void Freeze() {}
    void Thaw() {}

    int GetDPI() const { return 96; }
    double GetDPIScaleFactor() const { return 1.0; }
    double GetContentScaleFactor() const { return 1.0; }

    void SetFocus() {}
    bool HasFocus() const { return false; }

    virtual void Destroy() { delete this; }
    bool Close(bool force = false) { return true; }

    void CallAfter(std::function<void()> fn) { fn(); }

    void SetSizer(class wxSizer*, bool deleteOld = true) {}
    class wxSizer* GetSizer() const { return nullptr; }

private:
    wxWindowID m_id = wxID_ANY;
};

class wxControl : public wxWindow
{
public:
    wxControl() = default;
    wxControl(wxWindow* parent, wxWindowID id = wxID_ANY) : wxWindow(parent, id) {}
};

class wxDC
{
public:
    virtual ~wxDC() = default;
    void SetPen(const wxPen&) {}
    void SetBrush(const wxBrush&) {}
    void SetFont(const wxFont&) {}
    void DrawText(const wxString&, int, int) {}
    void DrawLine(int, int, int, int) {}
    void DrawRectangle(int, int, int, int) {}
    void DrawCircle(int, int, int) {}
    wxSize GetTextExtent(const wxString&) const { return wxSize(); }
};

#include "bmpbndl.h"

extern const wxPoint wxDefaultPosition;
extern const wxSize wxDefaultSize;
// wxEmptyString is defined as a macro in string.h
