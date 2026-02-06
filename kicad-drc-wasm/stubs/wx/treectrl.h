#pragma once
#include "window.h"
class wxTreeItemId {
public:
    wxTreeItemId() : m_id(nullptr) {}
    wxTreeItemId(void* id) : m_id(id) {}
    bool IsOk() const { return m_id != nullptr; }
    void* m_id;
};
class wxTreeCtrl : public wxWindow {};
class wxTreeEvent : public wxEvent {};
