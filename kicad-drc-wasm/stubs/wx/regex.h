#pragma once

#include "string.h"
#include <regex>

enum {
    wxRE_DEFAULT = 0,
    wxRE_EXTENDED = 0,
    wxRE_BASIC = 0,
    wxRE_ADVANCED = 0,
    wxRE_ICASE = 1,
    wxRE_NOSUB = 2,
    wxRE_NEWLINE = 4
};

class wxRegEx
{
public:
    wxRegEx() = default;
    wxRegEx(const wxString& pattern, int flags = wxRE_DEFAULT) { Compile(pattern, flags); }

    bool Compile(const wxString& pattern, int flags = wxRE_DEFAULT) {
        try {
            auto f = std::regex::ECMAScript;
            if(flags & wxRE_ICASE) f |= std::regex::icase;
            m_regex = std::regex(pattern.c_str(), f);
            m_valid = true;
        } catch(...) {
            m_valid = false;
        }
        return m_valid;
    }

    bool IsValid() const { return m_valid; }

    bool Matches(const wxString& text) const {
        if(!m_valid) return false;
        std::string s = text.c_str();
        return std::regex_search(s, m_match, m_regex);
    }

    bool GetMatch(size_t* start, size_t* len, size_t index = 0) const {
        if(index >= m_match.size()) return false;
        if(start) *start = m_match.position(index);
        if(len) *len = m_match.length(index);
        return true;
    }

    wxString GetMatch(const wxString& text, size_t index = 0) const {
        if(!m_valid) return wxString();
        std::string s = text.c_str();
        if(std::regex_search(s, m_match, m_regex) && index < m_match.size())
            return wxString(m_match[index].str());
        return wxString();
    }

    size_t GetMatchCount() const { return m_match.size(); }

    int Replace(wxString* text, const wxString& replacement, size_t maxMatches = 0) const {
        if(!m_valid || !text) return 0;
        std::string s = text->c_str();
        std::string r = std::regex_replace(s, m_regex, std::string(replacement.c_str()));
        *text = wxString(r);
        return (s != r) ? 1 : 0;
    }

    int ReplaceAll(wxString* text, const wxString& replacement) const {
        return Replace(text, replacement, 0);
    }

private:
    std::regex m_regex;
    mutable std::smatch m_match;
    bool m_valid = false;
};
