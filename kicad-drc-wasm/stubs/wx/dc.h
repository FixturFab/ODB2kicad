#pragma once

#include "gdicmn.h"
#include "string.h"

// wxAffineMatrix2D stub
class wxAffineMatrix2D
{
public:
    wxAffineMatrix2D() = default;
    void Set(double a, double b, double c, double d, double tx, double ty) {}
    void Translate(double dx, double dy) {}
    void Scale(double xScale, double yScale) {}
    void Rotate(double angle) {}
    void Concat(const wxAffineMatrix2D&) {}
    void Invert() {}
    bool IsIdentity() const { return true; }
};

class wxDC
{
public:
    virtual ~wxDC() = default;
    virtual void SetPen(const wxPen&) {}
    virtual void SetBrush(const wxBrush&) {}
    virtual void SetFont(const wxFont&) {}
    virtual void SetTextForeground(const wxColour&) {}
    virtual void SetTextBackground(const wxColour&) {}
    virtual void SetBackground(const wxBrush&) {}
    virtual void SetLogicalFunction(int) {}
    virtual void DrawLine(int, int, int, int) {}
    virtual void DrawRectangle(int, int, int, int) {}
    virtual void DrawText(const wxString&, int, int) {}
    virtual void DrawCircle(int, int, int) {}
    virtual void DrawBitmap(const wxBitmap&, int, int, bool = false) {}
    virtual wxSize GetTextExtent(const wxString&) const { return wxSize(0, 0); }
    virtual int GetCharHeight() const { return 12; }
    virtual int GetCharWidth() const { return 8; }
    void GetTextExtent(const wxString&, int* w, int* h) const { if(w) *w = 0; if(h) *h = 0; }
    void GetUserScale(double* x, double* y) const { if(x) *x = 1.0; if(y) *y = 1.0; }
    void SetUserScale(double, double) {}
    void GetLogicalOrigin(int* x, int* y) const { if(x) *x = 0; if(y) *y = 0; }
    void SetLogicalOrigin(int, int) {}
    void SetClippingRegion(int, int, int, int) {}
    void DestroyClippingRegion() {}
    void Clear() {}
    bool CanUseTransformMatrix() const { return false; }
    wxAffineMatrix2D GetTransformMatrix() const { return wxAffineMatrix2D(); }
    bool SetTransformMatrix(const wxAffineMatrix2D&) { return false; }
    void ResetTransformMatrix() {}
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
