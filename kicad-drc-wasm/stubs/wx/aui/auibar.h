#pragma once

#include "../event.h"
#include "../gdicmn.h"
#include "../bmpbndl.h"
#include "../sizer.h"
#include "../timer.h"
#include "../button.h"
#include "../menu.h"

class wxAuiToolBarItem
{
public:
    wxAuiToolBarItem() = default;
    void SetKind(int) {}
    int GetKind() const { return 0; }
    void SetId(int) {}
    int GetId() const { return 0; }
    void SetLabel(const wxString&) {}
    wxString GetLabel() const { return wxString(); }
    void SetBitmap(const wxBitmapBundle&) {}
    wxBitmapBundle GetBitmap() const { return wxBitmapBundle(); }
    void SetDisabledBitmap(const wxBitmapBundle&) {}
    wxBitmapBundle GetDisabledBitmap() const { return wxBitmapBundle(); }
    void SetShortHelp(const wxString&) {}
    wxString GetShortHelp() const { return wxString(); }
    void SetLongHelp(const wxString&) {}
    wxString GetLongHelp() const { return wxString(); }
    void SetActive(bool) {}
    bool IsActive() const { return false; }
    void SetHasDropDown(bool) {}
    bool HasDropDown() const { return false; }
    void SetSticky(bool) {}
    bool IsSticky() const { return false; }
    void SetUserData(long) {}
    long GetUserData() const { return 0; }
    void SetState(int) {}
    int GetState() const { return 0; }
    void SetWindow(wxWindow*) {}
    wxWindow* GetWindow() const { return nullptr; }
};

class wxAuiToolBarEvent : public wxCommandEvent
{
public:
    wxAuiToolBarEvent(wxEventType type = wxEVT_NULL, int id = 0) : wxCommandEvent(type, id) {}
    wxEvent* Clone() const override { return new wxAuiToolBarEvent(*this); }
    bool IsDropDownClicked() const { return false; }
    wxPoint GetClickPoint() const { return wxPoint(0, 0); }
    wxRect GetItemRect() const { return wxRect(0, 0, 0, 0); }
    int GetToolId() const { return GetId(); }
};

#define wxEVT_AUITOOLBAR_TOOL_DROPDOWN 200
#define wxEVT_AUITOOLBAR_OVERFLOW_CLICK 201
#define wxEVT_AUITOOLBAR_RIGHT_CLICK 202
#define wxEVT_AUITOOLBAR_MIDDLE_CLICK 203
#define wxEVT_AUITOOLBAR_BEGIN_DRAG 204
