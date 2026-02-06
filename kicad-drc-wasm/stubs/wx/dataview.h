#pragma once
#include "window.h"
#include "variant.h"
#include "colour.h"
#include <vector>
#include <cstdint>
template<typename T> using wxVector = std::vector<T>;
typedef uintptr_t wxUIntPtr;
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
class wxDataViewItemAttr {
public:
    void SetColour(const wxColour&) {}
    void SetBold(bool) {}
    void SetItalic(bool) {}
    void SetStrikethrough(bool) {}
    void SetBackgroundColour(const wxColour&) {}
    bool HasColour() const { return false; }
    bool HasFont() const { return false; }
    bool HasBackgroundColour() const { return false; }
    bool GetBold() const { return false; }
    bool IsDefault() const { return true; }
};
#define wxDATAVIEW_CELL_ACTIVATABLE 1
#define wxDATAVIEW_CELL_EDITABLE 2
#define wxDATAVIEW_CELL_INERT 0
#define wxCOL_WIDTH_DEFAULT -1
#define wxCOL_WIDTH_AUTOSIZE -2

class wxDataViewColumn {
public:
    wxDataViewColumn() = default;
    void SetHidden(bool) {}
    bool IsHidden() const { return false; }
    void SetWidth(int) {}
    int GetWidth() const { return 0; }
};
class wxDataViewModel : public wxEvtHandler {
public:
    virtual ~wxDataViewModel() = default;
    void ItemAdded(const wxDataViewItem&, const wxDataViewItem&) {}
    void ItemDeleted(const wxDataViewItem&, const wxDataViewItem&) {}
    void ItemChanged(const wxDataViewItem&) {}
    void Cleared() {}
    virtual unsigned int GetChildren(const wxDataViewItem&, wxDataViewItemArray&) const { return 0; }
    virtual bool HasContainerColumns(const wxDataViewItem&) const { return false; }
    virtual bool IsContainer(const wxDataViewItem&) const { return false; }
    virtual wxDataViewItem GetParent(const wxDataViewItem&) const { return wxDataViewItem(); }
    virtual unsigned int GetColumnCount() const { return 0; }
    virtual wxString GetColumnType(unsigned int) const { return wxString(); }
    virtual void GetValue(wxVariant&, const wxDataViewItem&, unsigned int) const {}
    virtual bool SetValue(const wxVariant&, const wxDataViewItem&, unsigned int) { return false; }
    virtual bool GetAttr(const wxDataViewItem&, unsigned int, wxDataViewItemAttr&) const { return false; }
    virtual bool HasValue(const wxDataViewItem&, unsigned int) const { return true; }
};
template<typename T>
class wxObjectDataPtr {
public:
    wxObjectDataPtr() : m_ptr(nullptr) {}
    wxObjectDataPtr(T* ptr) : m_ptr(ptr) {}
    wxObjectDataPtr(const wxObjectDataPtr& other) : m_ptr(other.m_ptr) {}
    ~wxObjectDataPtr() = default;
    T* get() const { return m_ptr; }
    T* operator->() const { return m_ptr; }
    T& operator*() const { return *m_ptr; }
    operator bool() const { return m_ptr != nullptr; }
    wxObjectDataPtr& operator=(const wxObjectDataPtr& other) { m_ptr = other.m_ptr; return *this; }
    wxObjectDataPtr& operator=(T* ptr) { m_ptr = ptr; return *this; }
private:
    T* m_ptr;
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
class wxDataViewIconText {
public:
    wxDataViewIconText() = default;
    wxDataViewIconText(const wxString&, const wxBitmapBundle& = wxBitmapBundle()) {}
    const wxString& GetText() const { static wxString s; return s; }
    void SetText(const wxString&) {}
    operator wxVariant() const { return wxVariant(); }
};

class wxDataViewListCtrl : public wxDataViewCtrl {
public:
    wxDataViewListCtrl() = default;
    wxDataViewListCtrl(wxWindow*, wxWindowID, const wxPoint& = wxPoint(),
                       const wxSize& = wxSize(), long = 0) {}
    wxDataViewColumn* AppendToggleColumn(const wxString&, int mode = 0, int w = -1, int align = 0) { return nullptr; }
    wxDataViewColumn* AppendTextColumn(const wxString&, int mode = 0, int w = -1, int align = 0) { return nullptr; }
    wxDataViewColumn* AppendIconTextColumn(const wxString&, int mode = 0, int w = -1, int align = 0) { return nullptr; }
    void AppendItem(const wxVector<wxVariant>&, wxUIntPtr = 0) {}
    void DeleteAllItems() {}
    unsigned int GetItemCount() const { return 0; }
    wxDataViewColumn* GetColumn(unsigned int) const { return nullptr; }
    int GetColumnCount_() const { return 0; }
    void SetToggleValue(bool, unsigned int, unsigned int) {}
    bool GetToggleValue(unsigned int, unsigned int) const { return false; }
    void SetTextValue(const wxString&, unsigned int, unsigned int) {}
    wxString GetTextValue(unsigned int, unsigned int) const { return wxString(); }
    int GetSelectedRow() const { return -1; }
    void SelectRow(unsigned int) {}
    wxColour GetBackgroundColour() const { return wxColour(); }
    int FromDIP(int v) const { return v; }
    wxSize FromDIP(const wxSize& s) const { return s; }
    void SetIndent(int) {}
    void SetMinClientSize(const wxSize&) {}
    wxSize GetMinClientSize() const { return wxSize(); }
};

class wxDataViewEvent : public wxEvent {
public:
    wxDataViewItem GetItem() const { return wxDataViewItem(); }
    wxDataViewColumn* GetColumn() const { return nullptr; }
    int GetModelColumn() const { return 0; }
};
