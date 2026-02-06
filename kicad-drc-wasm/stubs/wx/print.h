#pragma once
#include "dc.h"
class wxPrintout {
public:
    wxPrintout(const wxString& = wxString()) {}
    virtual ~wxPrintout() = default;
    virtual bool OnPrintPage(int) { return false; }
    virtual void GetPageInfo(int*, int*, int*, int*) {}
    virtual bool HasPage(int) { return false; }
    wxDC* GetDC() const { return nullptr; }
};
class wxPrinter {
public:
    wxPrinter() {}
    bool Print(wxWindow*, wxPrintout*, bool = true) { return false; }
    static wxString GetLastError() { return wxString(); }
};
class wxPrintData {};
class wxPrintPreview {};
