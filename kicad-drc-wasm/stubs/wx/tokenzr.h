#pragma once

#include "string.h"
#include <vector>
#include <sstream>

enum wxStringTokenizerMode {
    wxTOKEN_DEFAULT = 0,
    wxTOKEN_RET_EMPTY,
    wxTOKEN_RET_EMPTY_ALL,
    wxTOKEN_RET_DELIMS,
    wxTOKEN_STRTOK
};

class wxStringTokenizer
{
public:
    wxStringTokenizer() : m_pos(0) {}
    wxStringTokenizer(const wxString& str, const wxString& delims = wxString(" \t\r\n"),
                      wxStringTokenizerMode mode = wxTOKEN_DEFAULT)
        : m_str(str), m_delims(delims), m_pos(0), m_mode(mode) {}

    void SetString(const wxString& str, const wxString& delims = wxString(" \t\r\n"),
                   wxStringTokenizerMode mode = wxTOKEN_DEFAULT) {
        m_str = str; m_delims = delims; m_pos = 0; m_mode = mode;
    }

    bool HasMoreTokens() const {
        if(m_mode == wxTOKEN_RET_EMPTY || m_mode == wxTOKEN_RET_EMPTY_ALL)
            return m_pos <= m_str.length();
        size_t pos = m_pos;
        while(pos < m_str.length() && m_delims.find(m_str[pos]) != wxString::npos)
            pos++;
        return pos < m_str.length();
    }

    wxString GetNextToken() {
        if(m_mode == wxTOKEN_STRTOK || m_mode == wxTOKEN_DEFAULT) {
            while(m_pos < m_str.length() && m_delims.find(m_str[m_pos]) != wxString::npos)
                m_pos++;
        }
        if(m_pos >= m_str.length()) {
            m_pos = m_str.length() + 1;
            return wxString();
        }
        size_t start = m_pos;
        while(m_pos < m_str.length() && m_delims.find(m_str[m_pos]) == wxString::npos)
            m_pos++;
        wxString token = m_str.Mid(start, m_pos - start);
        if(m_mode != wxTOKEN_STRTOK && m_mode != wxTOKEN_DEFAULT)
            m_pos++;  // skip one delimiter
        return token;
    }

    size_t CountTokens() const {
        wxStringTokenizer tmp(m_str, m_delims, m_mode);
        size_t count = 0;
        while(tmp.HasMoreTokens()) { tmp.GetNextToken(); count++; }
        return count;
    }

    size_t GetPosition() const { return m_pos; }
    wxString GetString() const { return m_str; }

private:
    wxString m_str;
    wxString m_delims;
    size_t m_pos;
    wxStringTokenizerMode m_mode = wxTOKEN_DEFAULT;
};

inline wxArrayString wxSplit(const wxString& str, wxChar sep, wxChar escape = 0) {
    wxArrayString result;
    wxStringTokenizer tok(str, wxString(1, sep), wxTOKEN_RET_EMPTY_ALL);
    while(tok.HasMoreTokens()) result.Add(tok.GetNextToken());
    return result;
}

inline wxString wxJoin(const wxArrayString& arr, wxChar sep) {
    wxString result;
    for(size_t i = 0; i < arr.size(); i++) {
        if(i > 0) result += sep;
        result += arr[i];
    }
    return result;
}
