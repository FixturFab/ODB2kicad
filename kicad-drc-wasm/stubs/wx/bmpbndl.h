#pragma once

#include "gdicmn.h"

class wxBitmapBundle
{
public:
    wxBitmapBundle() = default;
    wxBitmapBundle(const wxBitmap& bmp) {}
    wxBitmapBundle(const wxImage& img) {}
    bool IsOk() const { return false; }
    wxSize GetDefaultSize() const { return wxSize(0, 0); }
    wxSize GetPreferredBitmapSizeFor(const wxWindow*) const { return wxSize(0, 0); }
    wxSize GetPreferredBitmapSizeAtScale(double) const { return wxSize(0, 0); }
    wxBitmap GetBitmap(const wxSize& sz) const { return wxBitmap(); }
    wxBitmap GetBitmapFor(const wxWindow*) const { return wxBitmap(); }

    static wxBitmapBundle FromBitmap(const wxBitmap& bmp) { return wxBitmapBundle(bmp); }
    static wxBitmapBundle FromImage(const wxImage& img) { return wxBitmapBundle(img); }
    static wxBitmapBundle FromSVG(const char*, const wxSize&) { return wxBitmapBundle(); }
    static wxBitmapBundle FromSVGFile(const wxString&, const wxSize&) { return wxBitmapBundle(); }
};
