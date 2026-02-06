#pragma once

// Minimal wxString stub for Emscripten/WASM build
// Maps wxString to a wrapper around std::string

#include <string>
#include <cstring>
#include <cstdio>
#include <cstdarg>
#include <vector>
#include <algorithm>
#include <iostream>
#include <sstream>
#include <functional>

#include "defs.h"
#include "debug.h"
#include "buffer.h"

// Forward declare wxMBConv for constructor
class wxMBConv;

// wxChar is just char in our stub
typedef char wxChar;
typedef unsigned char wxUChar;
typedef char wxSChar;
typedef char wxUniChar;

inline bool wxIsdigit(wxUniChar c) { return isdigit(static_cast<unsigned char>(c)); }

// Forward declare
class wxString;

// wxCStrData - helper for c_str() return
class wxCStrData
{
public:
    wxCStrData(const wxString* str);
    operator const char*() const;
    const char* AsChar() const;
private:
    const wxString* m_str;
};

class wxString
{
public:
    typedef std::string::iterator iterator;
    typedef std::string::const_iterator const_iterator;
    typedef std::string::reverse_iterator reverse_iterator;
    typedef std::string::const_reverse_iterator const_reverse_iterator;
    typedef std::string::size_type size_type;
    typedef char value_type;
    typedef char char_type;

    static const size_type npos = std::string::npos;

    wxString() = default;
    wxString(const char* s) : m_str(s ? s : "") {}
    wxString(const char* s, size_t n) : m_str(s, n) {}
    wxString(const std::string& s) : m_str(s) {}
    wxString(const wxString&) = default;
    wxString(wxString&&) = default;
    wxString(size_t n, char c) : m_str(n, c) {}
    wxString(const_iterator b, const_iterator e) : m_str(b, e) {}
    // Constructors accepting wxMBConv (encoding converter) - ignore encoding, treat as UTF8
    wxString(const char* s, const wxMBConv&) : m_str(s ? s : "") {}
    wxString(const char* s, const wxMBConv&, size_t n) : m_str(s ? s : "", n) {}
    wxString(const wchar_t* s) : m_str() { if(s) { while(*s) { m_str += (char)*s; s++; } } }
    wxString(const wchar_t* s, const wxMBConv&) : wxString(s) {}
    wxString(const wxCStrData& s);

    wxString& operator=(const wxString&) = default;
    wxString& operator=(wxString&&) = default;
    wxString& operator=(const char* s) { m_str = s ? s : ""; return *this; }
    wxString& operator=(const std::string& s) { m_str = s; return *this; }
    wxString& operator=(char c) { m_str = c; return *this; }

    // Conversions
    const char* c_str() const { return m_str.c_str(); }
    wxCharBuffer utf8_str() const { return wxCharBuffer(m_str.c_str()); }
    wxCharBuffer mb_str() const { return wxCharBuffer(m_str.c_str()); }
    const char* wc_str() const { return m_str.c_str(); }
    wxCharBuffer fn_str() const { return wxCharBuffer(m_str.c_str()); }
    wxCStrData GetData() const { return wxCStrData(this); }
    std::string ToStdString() const { return m_str; }
    const std::string& ToStdString_() const { return m_str; }
    operator const std::string&() const { return m_str; }
    wxCStrData wx_str() const { return wxCStrData(this); }

    std::string utf8_string() const { return m_str; }
    std::string ToUTF8() const { return m_str; }

    // Static constructors
    static wxString FromUTF8(const char* s) { return wxString(s); }
    static wxString FromUTF8(const char* s, size_t len) { return wxString(s, len); }
    static wxString FromUTF8(const std::string& s) { return wxString(s); }
    static wxString FromAscii(const char* s) { return wxString(s); }
    static wxString From8BitData(const char* s) { return wxString(s); }
    static wxString From8BitData(const char* s, size_t len) { return wxString(s, len); }

    // Comparison
    int Cmp(const wxString& s) const { return m_str.compare(s.m_str); }
    int CmpNoCase(const wxString& s) const {
        std::string a = m_str, b = s.m_str;
        std::transform(a.begin(), a.end(), a.begin(), ::tolower);
        std::transform(b.begin(), b.end(), b.begin(), ::tolower);
        return a.compare(b);
    }
    bool IsSameAs(const wxString& s, bool caseSensitive = true) const {
        return caseSensitive ? m_str == s.m_str : CmpNoCase(s) == 0;
    }
    bool IsSameAs(char c, bool caseSensitive = true) const {
        if (m_str.size() != 1) return false;
        return caseSensitive ? m_str[0] == c : ::tolower(m_str[0]) == ::tolower(c);
    }

    bool operator==(const wxString& o) const { return m_str == o.m_str; }
    bool operator==(const char* s) const { return m_str == (s ? s : ""); }
    bool operator!=(const wxString& o) const { return m_str != o.m_str; }
    bool operator!=(const char* s) const { return m_str != (s ? s : ""); }
    bool operator<(const wxString& o) const { return m_str < o.m_str; }
    bool operator>(const wxString& o) const { return m_str > o.m_str; }
    bool operator<=(const wxString& o) const { return m_str <= o.m_str; }
    bool operator>=(const wxString& o) const { return m_str >= o.m_str; }

    // Concatenation
    wxString operator+(const wxString& o) const { return wxString(m_str + o.m_str); }
    wxString operator+(const char* s) const { return wxString(m_str + (s ? s : "")); }
    wxString operator+(char c) const { return wxString(m_str + c); }
    wxString& operator+=(const wxString& o) { m_str += o.m_str; return *this; }
    wxString& operator+=(const char* s) { if(s) m_str += s; return *this; }
    wxString& operator+=(char c) { m_str += c; return *this; }
    wxString& operator<<(const wxString& s) { m_str += s.m_str; return *this; }
    wxString& operator<<(const char* s) { if(s) m_str += s; return *this; }
    wxString& operator<<(int n) { m_str += std::to_string(n); return *this; }
    wxString& operator<<(unsigned int n) { m_str += std::to_string(n); return *this; }
    wxString& operator<<(long n) { m_str += std::to_string(n); return *this; }
    wxString& operator<<(unsigned long n) { m_str += std::to_string(n); return *this; }
    wxString& operator<<(long long n) { m_str += std::to_string(n); return *this; }
    wxString& operator<<(double d) { m_str += std::to_string(d); return *this; }
    wxString& operator<<(char c) { m_str += c; return *this; }

    friend wxString operator+(const char* s, const wxString& o) { return wxString(std::string(s ? s : "") + o.m_str); }
    friend wxString operator+(char c, const wxString& o) { return wxString(std::string(1, c) + o.m_str); }

    // Size/empty
    size_t length() const { return m_str.length(); }
    size_t Length() const { return m_str.length(); }
    size_t Len() const { return m_str.length(); }
    size_t size() const { return m_str.size(); }
    bool empty() const { return m_str.empty(); }
    bool IsEmpty() const { return m_str.empty(); }
    bool IsNull() const { return m_str.empty(); }
    void Empty() { m_str.clear(); }
    void Clear() { m_str.clear(); }
    void clear() { m_str.clear(); }

    // Access
    char operator[](size_t i) const { return m_str[i]; }
    char& operator[](size_t i) { return m_str[i]; }
    char GetChar(size_t i) const { return m_str[i]; }
    char Last() const { return m_str.back(); }

    // Searching
    size_t Find(const wxString& s) const { return m_str.find(s.m_str); }
    size_t Find(char c, bool fromEnd = false) const {
        return fromEnd ? m_str.rfind(c) : m_str.find(c);
    }
    size_t find(const wxString& s, size_t pos = 0) const { return m_str.find(s.m_str, pos); }
    size_t find(char c, size_t pos = 0) const { return m_str.find(c, pos); }
    size_t find(const char* s, size_t pos = 0) const { return m_str.find(s, pos); }
    size_t rfind(char c, size_t pos = npos) const { return m_str.rfind(c, pos); }
    size_t find_first_of(const char* s, size_t pos = 0) const { return m_str.find_first_of(s, pos); }
    size_t find_first_of(const wxString& s, size_t pos = 0) const { return m_str.find_first_of(s.m_str, pos); }
    size_t find_last_of(const char* s, size_t pos = npos) const { return m_str.find_last_of(s, pos); }
    size_t find_first_not_of(const char* s, size_t pos = 0) const { return m_str.find_first_not_of(s, pos); }
    bool Contains(const wxString& s) const { return m_str.find(s.m_str) != std::string::npos; }

    // Modification
    wxString& Append(const wxString& s) { m_str += s.m_str; return *this; }
    wxString& Append(const char* s) { if(s) m_str += s; return *this; }
    wxString& Append(char c, size_t count = 1) { m_str.append(count, c); return *this; }
    wxString& Prepend(const wxString& s) { m_str.insert(0, s.m_str); return *this; }
    wxString& insert(size_t pos, const wxString& s) { m_str.insert(pos, s.m_str); return *this; }

    void Truncate(size_t len) { if(len < m_str.size()) m_str.resize(len); }
    wxString& Trim(bool fromRight = true) {
        if(fromRight) {
            auto pos = m_str.find_last_not_of(" \t\r\n");
            if(pos != std::string::npos) m_str.erase(pos + 1);
            else m_str.clear();
        } else {
            auto pos = m_str.find_first_not_of(" \t\r\n");
            if(pos != std::string::npos) m_str.erase(0, pos);
            else m_str.clear();
        }
        return *this;
    }
    wxString& Pad(size_t count, char pad = ' ', bool fromRight = true) {
        if(fromRight) m_str.append(count, pad);
        else m_str.insert(0, count, pad);
        return *this;
    }

    wxString& Remove(size_t pos) { m_str.erase(pos); return *this; }
    wxString& Remove(size_t pos, size_t len) { m_str.erase(pos, len); return *this; }
    wxString& RemoveLast(size_t n = 1) { if(n <= m_str.size()) m_str.erase(m_str.size() - n); return *this; }
    wxString& erase(size_t pos = 0, size_t len = npos) { m_str.erase(pos, len); return *this; }
    size_t Replace(char from, const wxString& to, bool replaceAll = true) {
        return Replace(wxString(1, from), to, replaceAll);
    }
    size_t Replace(const wxString& from, char to, bool replaceAll = true) {
        return Replace(from, wxString(1, to), replaceAll);
    }
    size_t Replace(char from, char to, bool replaceAll = true) {
        return Replace(wxString(1, from), wxString(1, to), replaceAll);
    }
    size_t Replace(const wxString& from, const wxString& to, bool replaceAll = true) {
        size_t count = 0;
        size_t pos = 0;
        while ((pos = m_str.find(from.m_str, pos)) != std::string::npos) {
            m_str.replace(pos, from.m_str.length(), to.m_str);
            pos += to.m_str.length();
            count++;
            if (!replaceAll) break;
        }
        return count;
    }

    void reserve(size_t n) { m_str.reserve(n); }

    // Extraction
    wxString Mid(size_t first, size_t count = npos) const {
        return wxString(m_str.substr(first, count));
    }
    wxString Left(size_t count) const { return wxString(m_str.substr(0, count)); }
    wxString Right(size_t count) const {
        if(count >= m_str.size()) return *this;
        return wxString(m_str.substr(m_str.size() - count));
    }
    wxString substr(size_t pos = 0, size_t len = npos) const {
        return wxString(m_str.substr(pos, len));
    }
    wxString SubString(size_t from, size_t to) const {
        return wxString(m_str.substr(from, to - from + 1));
    }

    wxString BeforeFirst(char c) const {
        auto pos = m_str.find(c);
        return pos == std::string::npos ? *this : wxString(m_str.substr(0, pos));
    }
    wxString BeforeLast(char c) const {
        auto pos = m_str.rfind(c);
        return pos == std::string::npos ? wxString() : wxString(m_str.substr(0, pos));
    }
    wxString AfterFirst(char c) const {
        auto pos = m_str.find(c);
        return pos == std::string::npos ? wxString() : wxString(m_str.substr(pos + 1));
    }
    wxString AfterLast(char c) const {
        auto pos = m_str.rfind(c);
        return pos == std::string::npos ? *this : wxString(m_str.substr(pos + 1));
    }

    // Case
    wxString Upper() const {
        wxString result = *this;
        std::transform(result.m_str.begin(), result.m_str.end(), result.m_str.begin(), ::toupper);
        return result;
    }
    wxString Lower() const {
        wxString result = *this;
        std::transform(result.m_str.begin(), result.m_str.end(), result.m_str.begin(), ::tolower);
        return result;
    }
    wxString& MakeUpper() { std::transform(m_str.begin(), m_str.end(), m_str.begin(), ::toupper); return *this; }
    wxString& MakeLower() { std::transform(m_str.begin(), m_str.end(), m_str.begin(), ::tolower); return *this; }
    wxString Capitalize() const {
        wxString result = *this;
        if (!result.m_str.empty()) {
            result.m_str[0] = (char)::toupper((unsigned char)result.m_str[0]);
        }
        return result;
    }

    // Testing
    bool StartsWith(const wxString& prefix, wxString* rest = nullptr) const {
        if(m_str.compare(0, prefix.m_str.size(), prefix.m_str) != 0) return false;
        if(rest) *rest = wxString(m_str.substr(prefix.m_str.size()));
        return true;
    }
    bool EndsWith(const wxString& suffix, wxString* rest = nullptr) const {
        if(m_str.size() < suffix.m_str.size()) return false;
        if(m_str.compare(m_str.size() - suffix.m_str.size(), suffix.m_str.size(), suffix.m_str) != 0) return false;
        if(rest) *rest = wxString(m_str.substr(0, m_str.size() - suffix.m_str.size()));
        return true;
    }
    bool IsNumber() const {
        if(m_str.empty()) return false;
        for(size_t i = (m_str[0] == '-' || m_str[0] == '+') ? 1 : 0; i < m_str.size(); ++i)
            if(!isdigit(m_str[i])) return false;
        return true;
    }

    // Conversion
    long ToLong(long* val, int base = 10) const {
        try { *val = std::stol(m_str, nullptr, base); return true; }
        catch(...) { return false; }
    }
    bool ToULong(unsigned long* val, int base = 10) const {
        try { *val = std::stoul(m_str, nullptr, base); return true; }
        catch(...) { return false; }
    }
    bool ToCULong(unsigned long* val, int base = 10) const {
        try { *val = std::stoul(m_str, nullptr, base); return true; }
        catch(...) { return false; }
    }
    bool ToLongLong(long long* val, int base = 10) const {
        try { *val = std::stoll(m_str, nullptr, base); return true; }
        catch(...) { return false; }
    }
    bool ToDouble(double* val) const {
        try { *val = std::stod(m_str); return true; }
        catch(...) { return false; }
    }
    bool ToCDouble(double* val) const {
        try { *val = std::stod(m_str); return true; }
        catch(...) { return false; }
    }
    int ToInt(int* val, int base = 10) const {
        long l;
        if(!ToLong(&l, base)) return false;
        *val = (int)l;
        return true;
    }

    // Printf
    static wxString Format(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        char buf[4096];
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);
        return wxString(buf);
    }
    static wxString Format(const wxString& fmt, ...) {
        va_list args;
        va_start(args, fmt);
        char buf[4096];
        vsnprintf(buf, sizeof(buf), fmt.c_str(), args);
        va_end(args);
        return wxString(buf);
    }
    int Printf(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        char buf[4096];
        int n = vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);
        m_str = buf;
        return n;
    }
    int Printf(const wxString& fmt, ...) {
        va_list args;
        va_start(args, fmt);
        char buf[4096];
        int n = vsnprintf(buf, sizeof(buf), fmt.c_str(), args);
        va_end(args);
        m_str = buf;
        return n;
    }
    wxString& sprintf(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        char buf[4096];
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);
        m_str = buf;
        return *this;
    }

    // Iterators
    iterator begin() { return m_str.begin(); }
    iterator end() { return m_str.end(); }
    const_iterator begin() const { return m_str.begin(); }
    const_iterator end() const { return m_str.end(); }
    reverse_iterator rbegin() { return m_str.rbegin(); }
    reverse_iterator rend() { return m_str.rend(); }
    const_reverse_iterator rbegin() const { return m_str.rbegin(); }
    const_reverse_iterator rend() const { return m_str.rend(); }

    // Hash support
    struct StdHash {
        size_t operator()(const wxString& s) const { return std::hash<std::string>{}(s.m_str); }
    };

    // Stream support
    friend std::ostream& operator<<(std::ostream& os, const wxString& s) {
        return os << s.m_str;
    }

    // Internal
    const std::string& _str() const { return m_str; }
    std::string& _str() { return m_str; }

private:
    std::string m_str;
};

// wxCStrData implementation
inline wxCStrData::wxCStrData(const wxString* str) : m_str(str) {}
inline wxCStrData::operator const char*() const { return m_str->c_str(); }
inline const char* wxCStrData::AsChar() const { return m_str->c_str(); }

// Hash specialization
namespace std {
    template<> struct hash<wxString> {
        size_t operator()(const wxString& s) const { return hash<string>{}(s.c_str()); }
    };
}

// wxArrayString
class wxArrayString : public std::vector<wxString>
{
public:
    void Add(const wxString& s) { push_back(s); }
    size_t GetCount() const { return size(); }
    wxString& Item(size_t i) { return (*this)[i]; }
    const wxString& Item(size_t i) const { return (*this)[i]; }
    int Index(const wxString& s) const {
        for(size_t i = 0; i < size(); i++)
            if((*this)[i] == s) return (int)i;
        return -1;
    }
    void Remove(const wxString& s) {
        for(auto it = begin(); it != end(); ++it)
            if(*it == s) { erase(it); return; }
    }
    void RemoveAt(size_t i) { erase(begin() + i); }
    void Sort() { std::sort(begin(), end()); }
};

// String format helper
inline wxString wxString_Format(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char buf[4096];
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    return wxString(buf);
}

// wxCStrData -> wxString constructor
inline wxString::wxString(const wxCStrData& s) : m_str(s.AsChar()) {}

// Compatibility
#define wxEmptyString wxString()
#define wxSTRING_MAXLEN std::string::npos

// wxSafeConvertWX2MB - convert wxString to multibyte (trivial in our UTF8 build)
inline std::string wxSafeConvertWX2MB(const char* s) { return s ? std::string(s) : std::string(); }
inline std::string wxSafeConvertWX2MB(const wxString& s) { return std::string(s.c_str()); }
