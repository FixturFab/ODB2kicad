#pragma once

#include "window.h"

class wxSizerItem {};

class wxSizer
{
public:
    virtual ~wxSizer() = default;
    wxSizerItem* Add(wxWindow*, int = 0, int = 0, int = 0) { return nullptr; }
    wxSizerItem* Add(wxSizer*, int = 0, int = 0, int = 0) { return nullptr; }
    wxSizerItem* Add(int, int, int = 0, int = 0, int = 0) { return nullptr; }
    void Layout() {}
    void Fit(wxWindow*) {}
    void SetSizeHints(wxWindow*) {}
    wxSize GetMinSize() const { return wxSize(0, 0); }
};

class wxBoxSizer : public wxSizer
{
public:
    wxBoxSizer(int orient) {}
};

class wxStaticBoxSizer : public wxBoxSizer
{
public:
    wxStaticBoxSizer(int orient, wxWindow* parent, const wxString& label = wxString())
        : wxBoxSizer(orient) {}
};

class wxGridSizer : public wxSizer
{
public:
    wxGridSizer(int cols, int vgap = 0, int hgap = 0) {}
    wxGridSizer(int rows, int cols, int vgap, int hgap) {}
};

class wxFlexGridSizer : public wxGridSizer
{
public:
    wxFlexGridSizer(int cols, int vgap = 0, int hgap = 0) : wxGridSizer(cols, vgap, hgap) {}
    wxFlexGridSizer(int rows, int cols, int vgap, int hgap) : wxGridSizer(rows, cols, vgap, hgap) {}
    void AddGrowableCol(size_t, int = 0) {}
    void AddGrowableRow(size_t, int = 0) {}
};

class wxGridBagSizer : public wxFlexGridSizer
{
public:
    wxGridBagSizer(int vgap = 0, int hgap = 0) : wxFlexGridSizer(0, vgap, hgap) {}
};

class wxStdDialogButtonSizer : public wxBoxSizer
{
public:
    wxStdDialogButtonSizer() : wxBoxSizer(wxHORIZONTAL) {}
    void AddButton(wxWindow*) {}
    void Realize() {}
};

class wxWrapSizer : public wxBoxSizer
{
public:
    wxWrapSizer(int orient = wxHORIZONTAL, int flags = 0) : wxBoxSizer(orient) {}
};
