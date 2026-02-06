#pragma once
#include "../window.h"
#include <vector>

class wxPGChoiceEntry
{
public:
    wxPGChoiceEntry() = default;
    wxPGChoiceEntry(const wxString& label, int value = 0) : m_label(label), m_value(value) {}
    wxString GetText() const { return m_label; }
    int GetValue() const { return m_value; }
private:
    wxString m_label;
    int m_value = 0;
};

class wxPGChoices
{
public:
    wxPGChoices() = default;
    void Add(const wxString& label, int value = 0) {
        m_entries.push_back(wxPGChoiceEntry(label, value));
    }
    size_t GetCount() const { return m_entries.size(); }
    const wxPGChoiceEntry& operator[](size_t i) const { return m_entries[i]; }
    const wxPGChoiceEntry& Item(size_t i) const { return m_entries[i]; }
    bool IsOk() const { return !m_entries.empty(); }
    void Clear() { m_entries.clear(); }
    wxString GetLabel(size_t i) const { return i < m_entries.size() ? m_entries[i].GetText() : wxString(); }
    int GetValue(size_t i) const { return i < m_entries.size() ? m_entries[i].GetValue() : 0; }
    int Index(int value) const {
        for(size_t i = 0; i < m_entries.size(); i++)
            if(m_entries[i].GetValue() == value) return (int)i;
        return -1;
    }
    wxArrayString GetLabels() const {
        wxArrayString labels;
        for(auto& e : m_entries) labels.Add(e.GetText());
        return labels;
    }
private:
    std::vector<wxPGChoiceEntry> m_entries;
};

class wxPGProperty {};
class wxPropertyGrid : public wxWindow {};
class wxPropertyGridManager : public wxWindow {};
class wxPropertyGridEvent : public wxEvent {};
