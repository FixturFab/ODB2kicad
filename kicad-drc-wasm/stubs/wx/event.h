#pragma once

#include "defs.h"
#include "string.h"
#include "gdicmn.h"
#include "datetime.h"
#include <functional>

// Forward declarations needed by kiway.h and others
class wxFrame;

// Event type
typedef int wxEventType;
#define wxDECLARE_EVENT(name, type) extern const wxEventType name
#define wxDEFINE_EVENT(name, type) const wxEventType name = __LINE__
#define wxEVT_NULL 0
#define wxEVT_COMMAND_BUTTON_CLICKED 1
#define wxEVT_MENU 2
#define wxEVT_CLOSE_WINDOW 3
#define wxEVT_SIZE 4
#define wxEVT_PAINT 5
#define wxEVT_CHAR 6
#define wxEVT_KEY_DOWN 7
#define wxEVT_KEY_UP 8
#define wxEVT_IDLE 9
#define wxEVT_THREAD 10
#define wxEVT_CHAR_HOOK 11
#define wxEVT_UPDATE_UI 13
#define wxEVT_ACTIVATE 14
#define wxEVT_SHOW 15
#define wxEVT_MAXIMIZE 16
#define wxEVT_ICONIZE 17
#define wxEVT_MOVE 18
#define wxEVT_SET_FOCUS 19
#define wxEVT_KILL_FOCUS 20
#define wxEVT_SCROLL_TOP 21
#define wxEVT_SCROLL_BOTTOM 22
#define wxEVT_SCROLL_LINEUP 23
#define wxEVT_SCROLL_LINEDOWN 24
#define wxEVT_SCROLL_PAGEUP 25
#define wxEVT_SCROLL_PAGEDOWN 26
#define wxEVT_SCROLL_THUMBTRACK 27
#define wxEVT_SCROLL_THUMBRELEASE 28
#define wxEVT_ERASE_BACKGROUND 29
#define wxEVT_ENTER_WINDOW 30
#define wxEVT_LEAVE_WINDOW 31
#define wxEVT_DROP_FILES 32
#define wxEVT_DPI_CHANGED 33

class wxEvent
{
public:
    wxEvent(int id = 0, wxEventType type = wxEVT_NULL) : m_id(id), m_type(type) {}
    virtual ~wxEvent() = default;
    virtual wxEvent* Clone() const { return new wxEvent(*this); }

    wxEventType GetEventType() const { return m_type; }
    void SetEventType(wxEventType type) { m_type = type; }
    int GetId() const { return m_id; }
    void SetId(int id) { m_id = id; }
    void Skip(bool skip = true) { m_skipped = skip; }
    bool GetSkipped() const { return m_skipped; }
    void StopPropagation() {}

private:
    int m_id;
    wxEventType m_type;
    bool m_skipped = false;
};

class wxCommandEvent : public wxEvent
{
public:
    wxCommandEvent(wxEventType type = wxEVT_NULL, int id = 0) : wxEvent(id, type) {}
    wxEvent* Clone() const override { return new wxCommandEvent(*this); }
    wxString GetString() const { return m_string; }
    void SetString(const wxString& s) { m_string = s; }
    int GetInt() const { return m_int; }
    void SetInt(int i) { m_int = i; }
    bool IsChecked() const { return m_int != 0; }
private:
    wxString m_string;
    int m_int = 0;
};

class wxThreadEvent : public wxEvent
{
public:
    wxThreadEvent(wxEventType type = wxEVT_THREAD, int id = 0) : wxEvent(id, type) {}
    wxEvent* Clone() const override { return new wxThreadEvent(*this); }
    wxString GetString() const { return m_string; }
    void SetString(const wxString& s) { m_string = s; }
    template<typename T> void SetPayload(const T&) {}
    template<typename T> T GetPayload() const { return T(); }
private:
    wxString m_string;
};

class wxEvtHandler
{
public:
    virtual ~wxEvtHandler() = default;
    virtual bool ProcessEvent(wxEvent& event) { return false; }
    void Bind(wxEventType, std::function<void(wxCommandEvent&)>, int = wxID_ANY) {}
    void Bind(wxEventType, std::function<void(wxEvent&)>, int = wxID_ANY) {}
    virtual void QueueEvent(wxEvent* event) { delete event; }
    void AddPendingEvent(const wxEvent& event) {}
    bool ProcessEventLocally(wxEvent& event) { return false; }
};

// Event table macros
#define wxBEGIN_EVENT_TABLE(cls, base)
#define wxEND_EVENT_TABLE()
#define EVT_MENU(id, fn)
#define EVT_BUTTON(id, fn)
#define EVT_SIZE(fn)
#define EVT_CLOSE(fn)
#define EVT_PAINT(fn)
#define EVT_IDLE(fn)
#define wxDECLARE_EVENT_TABLE()

class wxMouseEvent : public wxEvent
{
public:
    wxMouseEvent(wxEventType type = wxEVT_NULL) : wxEvent(0, type) {}
    wxEvent* Clone() const override { return new wxMouseEvent(*this); }
    int GetX() const { return 0; }
    int GetY() const { return 0; }
    wxPoint GetPosition() const { return wxPoint(0, 0); }
    bool LeftDown() const { return false; }
    bool LeftUp() const { return false; }
    bool RightDown() const { return false; }
    bool RightUp() const { return false; }
    bool MiddleDown() const { return false; }
    bool MiddleUp() const { return false; }
    bool LeftDClick() const { return false; }
    bool Moving() const { return false; }
    bool Dragging() const { return false; }
    bool LeftIsDown() const { return false; }
    bool RightIsDown() const { return false; }
    bool MiddleIsDown() const { return false; }
    bool ShiftDown() const { return false; }
    bool ControlDown() const { return false; }
    bool AltDown() const { return false; }
    int GetWheelRotation() const { return 0; }
    int GetWheelDelta() const { return 120; }
    bool Entering() const { return false; }
    bool Leaving() const { return false; }
};

class wxKeyEvent : public wxEvent
{
public:
    wxKeyEvent(wxEventType type = wxEVT_NULL) : wxEvent(0, type) {}
    wxEvent* Clone() const override { return new wxKeyEvent(*this); }
    int GetKeyCode() const { return 0; }
    int GetUnicodeKey() const { return 0; }
    bool ShiftDown() const { return false; }
    bool ControlDown() const { return false; }
    bool AltDown() const { return false; }
    bool MetaDown() const { return false; }
    bool RawControlDown() const { return false; }
    bool HasModifiers() const { return false; }
    bool HasAnyModifiers() const { return false; }
    int GetX() const { return 0; }
    int GetY() const { return 0; }
    wxPoint GetPosition() const { return wxPoint(); }
};

class wxSizeEvent : public wxEvent
{
public:
    wxSizeEvent(const wxSize& sz = wxSize(), int id = 0) : wxEvent(id, wxEVT_SIZE), m_size(sz) {}
    wxEvent* Clone() const override { return new wxSizeEvent(*this); }
    wxSize GetSize() const { return m_size; }
private:
    wxSize m_size;
};

class wxSysColourChangedEvent : public wxEvent
{
public:
    wxSysColourChangedEvent() : wxEvent(0, wxEVT_NULL) {}
    wxEvent* Clone() const override { return new wxSysColourChangedEvent(*this); }
};

#define wxEVT_LEFT_DOWN 20
#define wxEVT_LEFT_UP 21
#define wxEVT_RIGHT_DOWN 22
#define wxEVT_RIGHT_UP 23
#define wxEVT_MIDDLE_DOWN 24
#define wxEVT_MIDDLE_UP 25
#define wxEVT_MOTION 26
#define wxEVT_MOUSEWHEEL 27
#define wxEVT_LEFT_DCLICK 28
// wxEVT_ENTER_WINDOW and wxEVT_LEAVE_WINDOW defined above
#define wxEVT_SYS_COLOUR_CHANGED 50

class wxTimerEvent : public wxEvent
{
public:
    wxTimerEvent() : wxEvent(0, wxEVT_NULL) {}
    wxEvent* Clone() const override { return new wxTimerEvent(*this); }
};

class wxPaintEvent : public wxEvent
{
public:
    wxPaintEvent(int id = 0) : wxEvent(id, wxEVT_PAINT) {}
    wxEvent* Clone() const override { return new wxPaintEvent(*this); }
};

class wxFocusEvent : public wxEvent
{
public:
    wxFocusEvent(wxEventType type = wxEVT_SET_FOCUS, int id = 0) : wxEvent(id, type) {}
    wxEvent* Clone() const override { return new wxFocusEvent(*this); }
};

class wxIdleEvent : public wxEvent
{
public:
    wxIdleEvent() : wxEvent(0, wxEVT_IDLE) {}
    wxEvent* Clone() const override { return new wxIdleEvent(*this); }
    void RequestMore(bool needMore = true) {}
};

class wxShowEvent : public wxEvent
{
public:
    wxShowEvent(int id = 0, bool show = true) : wxEvent(id, wxEVT_SHOW) {}
    wxEvent* Clone() const override { return new wxShowEvent(*this); }
    bool IsShown() const { return true; }
};

class wxDPIChangedEvent : public wxEvent
{
public:
    wxDPIChangedEvent() : wxEvent(0, wxEVT_DPI_CHANGED) {}
    wxEvent* Clone() const override { return new wxDPIChangedEvent(*this); }
};

// wxLongLong - wrapper for 64-bit integer
class wxLongLong
{
public:
    wxLongLong() : m_val(0) {}
    wxLongLong(long long v) : m_val(v) {}
    long long GetValue() const { return m_val; }
    long ToLong() const { return (long)m_val; }
    operator long long() const { return m_val; }
private:
    long long m_val;
};

class wxULongLong
{
public:
    wxULongLong() : m_val(0) {}
    wxULongLong(unsigned long long v) : m_val(v) {}
    unsigned long long GetValue() const { return m_val; }
    unsigned long ToULong() const { return (unsigned long)m_val; }
    operator unsigned long long() const { return m_val; }
    wxULongLong& operator+=(const wxULongLong& o) { m_val += o.m_val; return *this; }
    wxULongLong& operator-=(const wxULongLong& o) { m_val -= o.m_val; return *this; }
    bool operator>(const wxULongLong& o) const { return m_val > o.m_val; }
    bool operator<(const wxULongLong& o) const { return m_val < o.m_val; }
    bool operator==(const wxULongLong& o) const { return m_val == o.m_val; }
private:
    unsigned long long m_val;
};

// wxPanelNameStr
inline const wxString wxPanelNameStr("panel");

class wxMenu;  // forward declaration

class wxMenuEvent : public wxEvent
{
public:
    wxMenuEvent(wxEventType type = wxEVT_NULL, int id = 0, wxMenu* menu = nullptr)
        : wxEvent(id, type), m_menu(menu) {}
    wxEvent* Clone() const override { return new wxMenuEvent(*this); }
    int GetMenuId() const { return GetId(); }
    wxMenu* GetMenu() const { return m_menu; }
    bool IsPopup() const { return false; }
private:
    wxMenu* m_menu = nullptr;
};

class wxUpdateUIEvent : public wxCommandEvent
{
public:
    wxUpdateUIEvent(int id = 0) : wxCommandEvent(wxEVT_NULL, id) {}
    wxEvent* Clone() const override { return new wxUpdateUIEvent(*this); }
    void Enable(bool enable) { m_enabled = enable; }
    void Check(bool check) { m_checked = check; }
    bool GetChecked() const { return m_checked; }
    bool GetEnabled() const { return m_enabled; }
    void Show(bool show) { m_shown = show; }
    bool GetShown() const { return m_shown; }
    void SetText(const wxString& text) { m_text = text; }
    wxString GetText() const { return m_text; }
private:
    bool m_enabled = true;
    bool m_checked = false;
    bool m_shown = true;
    wxString m_text;
};

class wxMoveEvent : public wxEvent
{
public:
    wxMoveEvent(const wxPoint& pos = wxPoint(), int id = 0) : wxEvent(id, wxEVT_NULL), m_pos(pos) {}
    wxEvent* Clone() const override { return new wxMoveEvent(*this); }
    wxPoint GetPosition() const { return m_pos; }
private:
    wxPoint m_pos;
};

class wxMaximizeEvent : public wxEvent
{
public:
    wxMaximizeEvent(int id = 0) : wxEvent(id, wxEVT_NULL) {}
    wxEvent* Clone() const override { return new wxMaximizeEvent(*this); }
};

class wxCloseEvent : public wxEvent
{
public:
    wxCloseEvent(wxEventType type = wxEVT_CLOSE_WINDOW, int id = 0) : wxEvent(id, type) {}
    wxEvent* Clone() const override { return new wxCloseEvent(*this); }
    void Veto(bool veto = true) { m_veto = veto; }
    bool GetVeto() const { return m_veto; }
    bool CanVeto() const { return m_canVeto; }
    void SetCanVeto(bool canVeto) { m_canVeto = canVeto; }
private:
    bool m_veto = false;
    bool m_canVeto = true;
};

class wxIconizeEvent : public wxEvent
{
public:
    wxIconizeEvent(int id = 0, bool iconized = true) : wxEvent(id, wxEVT_NULL), m_iconized(iconized) {}
    wxEvent* Clone() const override { return new wxIconizeEvent(*this); }
    bool IsIconized() const { return m_iconized; }
private:
    bool m_iconized;
};

class wxDropFilesEvent : public wxEvent
{
public:
    wxDropFilesEvent(wxEventType type = wxEVT_NULL, int noFiles = 0, wxString* files = nullptr)
        : wxEvent(0, type), m_noFiles(noFiles), m_files(files) {}
    wxEvent* Clone() const override { return new wxDropFilesEvent(*this); }
    int GetNumberOfFiles() const { return m_noFiles; }
    wxString* GetFiles() const { return m_files; }
private:
    int m_noFiles;
    wxString* m_files;
};

// wxDECLARE_EXPORTED_EVENT macro
#define wxDECLARE_EXPORTED_EVENT(export, name, type) extern const wxEventType name

// DECLARE_EVENT_TABLE - old-style macro (without wx prefix)
#define DECLARE_EVENT_TABLE() wxDECLARE_EVENT_TABLE()

// wxPostEvent
inline void wxPostEvent(wxEvtHandler* handler, const wxEvent& event) {}
