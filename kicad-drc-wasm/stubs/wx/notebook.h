#pragma once
#include "window.h"
class wxNotebook : public wxWindow {
public:
    wxNotebook() = default;
    wxNotebook(wxWindow*, wxWindowID, const wxPoint& = wxPoint(), const wxSize& = wxSize(), long = 0) {}
    int GetSelection() const { return 0; }
    void SetSelection(int) {}
    bool AddPage(wxWindow*, const wxString&, bool = false) { return true; }
    size_t GetPageCount() const { return 0; }
    wxWindow* GetPage(size_t) const { return nullptr; }
};
