#pragma once
#include "window.h"
class wxMenuItem {
public:
    wxMenuItem(void* = nullptr, int = wxID_ANY, const wxString& = wxString(),
               const wxString& = wxString(), int = 0) {}
    virtual ~wxMenuItem() = default;
    int GetId() const { return 0; }
    void Enable(bool = true) {}
    void Check(bool = true) {}
    bool IsChecked() const { return false; }
    void SetBitmap(const wxBitmap&) {}
    wxString GetItemLabelText() const { return wxString(); }
};
class wxMenu : public wxEvtHandler {
public:
    wxMenu(const wxString& = wxString(), long = 0) {}
    wxMenuItem* Append(int, const wxString&, const wxString& = wxString(), int = 0) { return nullptr; }
    wxMenuItem* Append(wxMenuItem*) { return nullptr; }
    wxMenuItem* AppendSeparator() { return nullptr; }
    wxMenu* AppendSubMenu(wxMenu*, const wxString&) { return nullptr; }
    wxMenuItem* AppendCheckItem(int, const wxString&, const wxString& = wxString()) { return nullptr; }
    wxMenuItem* AppendRadioItem(int, const wxString&, const wxString& = wxString()) { return nullptr; }
    void Enable(int, bool = true) {}
    void Check(int, bool = true) {}
    bool IsChecked(int) const { return false; }
    size_t GetMenuItemCount() const { return 0; }
    wxMenuItem* FindItem(int) const { return nullptr; }
    void Remove(wxMenuItem*) {}
    void Delete(wxMenuItem*) {}
    void Destroy(wxMenuItem*) {}
};
class wxMenuBar : public wxWindow {
public:
    wxMenuBar() = default;
    void Append(wxMenu*, const wxString&) {}
    wxMenu* GetMenu(size_t) const { return nullptr; }
    size_t GetMenuCount() const { return 0; }
};
