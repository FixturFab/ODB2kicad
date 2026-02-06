#pragma once
#include "window.h"
class wxDataViewItem {
public:
    wxDataViewItem() : m_id(nullptr) {}
    wxDataViewItem(void* id) : m_id(id) {}
    void* GetID() const { return m_id; }
    bool IsOk() const { return m_id != nullptr; }
    bool operator==(const wxDataViewItem& o) const { return m_id == o.m_id; }
    bool operator!=(const wxDataViewItem& o) const { return m_id != o.m_id; }
private:
    void* m_id;
};
class wxDataViewItemArray : public std::vector<wxDataViewItem> {};
class wxDataViewColumn {};
class wxDataViewModel : public wxEvtHandler {
public:
    virtual ~wxDataViewModel() = default;
    void ItemAdded(const wxDataViewItem&, const wxDataViewItem&) {}
    void ItemDeleted(const wxDataViewItem&, const wxDataViewItem&) {}
    void ItemChanged(const wxDataViewItem&) {}
    void Cleared() {}
};
class wxDataViewCtrl : public wxWindow {
public:
    wxDataViewCtrl() = default;
    wxDataViewItem GetSelection() const { return wxDataViewItem(); }
    int GetSelectedItemsCount() const { return 0; }
    void Select(const wxDataViewItem&) {}
    void Unselect(const wxDataViewItem&) {}
    void UnselectAll() {}
    void AssociateModel(wxDataViewModel*) {}
    wxDataViewColumn* AppendTextColumn(const wxString&, int = 0) { return nullptr; }
    void Expand(const wxDataViewItem&) {}
    void Collapse(const wxDataViewItem&) {}
    void EnsureVisible(const wxDataViewItem&, wxDataViewColumn* = nullptr) {}
};
class wxDataViewEvent : public wxEvent {
public:
    wxDataViewItem GetItem() const { return wxDataViewItem(); }
    wxDataViewColumn* GetColumn() const { return nullptr; }
    int GetModelColumn() const { return 0; }
};
