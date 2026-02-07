#pragma once
#include "string.h"
class wxURI {
public:
    wxURI() = default;
    wxURI(const wxString& uri) : m_uri(uri) {}
    wxString BuildURI() const { return m_uri; }
    wxString GetScheme() const { return wxString(); }
    wxString GetServer() const { return wxString(); }
    wxString GetPath() const { return m_uri; }
    wxString GetFragment() const { return wxString(); }
    wxString GetQuery() const { return wxString(); }
    bool Create(const wxString& uri) { m_uri = uri; return true; }
    bool HasScheme() const { return false; }
    bool HasServer() const { return false; }
    bool IsReference() const { return false; }
    static wxString Unescape(const wxString& s) { return s; }
private:
    wxString m_uri;
};

class wxURL : public wxURI {
public:
    wxURL() = default;
    wxURL(const wxString& url) : wxURI(url) {}
};
