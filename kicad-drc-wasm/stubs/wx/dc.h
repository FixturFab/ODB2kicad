#pragma once

#include "gdicmn.h"
#include "string.h"

class wxDC
{
public:
    virtual ~wxDC() = default;
    virtual void SetPen(const wxPen&) {}
    virtual void SetBrush(const wxBrush&) {}
    virtual void SetFont(const wxFont&) {}
    virtual void SetTextForeground(const wxColour&) {}
    virtual void SetTextBackground(const wxColour&) {}
    virtual void DrawLine(int, int, int, int) {}
    virtual void DrawRectangle(int, int, int, int) {}
    virtual void DrawText(const wxString&, int, int) {}
    virtual void DrawCircle(int, int, int) {}
    virtual wxSize GetTextExtent(const wxString&) const { return wxSize(0, 0); }
    virtual int GetCharHeight() const { return 12; }
    virtual int GetCharWidth() const { return 8; }
    void GetTextExtent(const wxString&, int* w, int* h) const { if(w) *w = 0; if(h) *h = 0; }
};

class wxMemoryDC : public wxDC {};
class wxClientDC : public wxDC {
public:
    wxClientDC(wxWindow* = nullptr) {}
};
class wxPaintDC : public wxDC {
public:
    wxPaintDC(wxWindow* = nullptr) {}
};
