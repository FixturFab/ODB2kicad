#pragma once

#include "string.h"
#include <map>

class wxConfigBase
{
public:
    virtual ~wxConfigBase() = default;
    static wxConfigBase* Get(bool createOnDemand = true) { return nullptr; }
    static wxConfigBase* Set(wxConfigBase*) { return nullptr; }
    static wxConfigBase* Create() { return nullptr; }
    static void DontCreateOnDemand() {}

    virtual bool Read(const wxString& key, wxString* value) const { return false; }
    virtual bool Read(const wxString& key, wxString* value, const wxString& defaultVal) const {
        *value = defaultVal; return false;
    }
    virtual bool Read(const wxString& key, long* value, long defaultVal = 0) const {
        *value = defaultVal; return false;
    }
    virtual bool Read(const wxString& key, int* value, int defaultVal = 0) const {
        *value = defaultVal; return false;
    }
    virtual bool Read(const wxString& key, double* value, double defaultVal = 0) const {
        *value = defaultVal; return false;
    }
    virtual bool Read(const wxString& key, bool* value, bool defaultVal = false) const {
        *value = defaultVal; return false;
    }

    wxString Read(const wxString& key, const wxString& defaultVal = wxString()) const {
        wxString val;
        Read(key, &val, defaultVal);
        return val;
    }

    virtual bool Write(const wxString&, const wxString&) { return false; }
    virtual bool Write(const wxString&, int) { return false; }
    virtual bool Write(const wxString&, long) { return false; }
    virtual bool Write(const wxString&, double) { return false; }
    virtual bool Write(const wxString&, bool) { return false; }

    virtual bool HasEntry(const wxString&) const { return false; }
    virtual bool HasGroup(const wxString&) const { return false; }
    virtual bool DeleteEntry(const wxString&, bool = true) { return false; }
    virtual bool DeleteGroup(const wxString&) { return false; }
    virtual bool DeleteAll() { return false; }
    virtual bool Flush(bool = false) { return true; }

    void SetPath(const wxString&) {}
    wxString GetPath() const { return wxString("/"); }

    virtual bool GetFirstEntry(wxString& str, long& index) const { return false; }
    virtual bool GetNextEntry(wxString& str, long& index) const { return false; }
    virtual bool GetFirstGroup(wxString& str, long& index) const { return false; }
    virtual bool GetNextGroup(wxString& str, long& index) const { return false; }
    virtual size_t GetNumberOfEntries(bool bRecursive = false) const { return 0; }
    virtual size_t GetNumberOfGroups(bool bRecursive = false) const { return 0; }
};

class wxFileConfig : public wxConfigBase
{
public:
    wxFileConfig(const wxString& appName = wxString(),
                 const wxString& vendorName = wxString(),
                 const wxString& localFilename = wxString(),
                 const wxString& globalFilename = wxString(),
                 long style = 0) {}
};

class wxConfig : public wxConfigBase
{
public:
    wxConfig(const wxString& appName = wxString(),
             const wxString& vendorName = wxString(),
             const wxString& localFilename = wxString(),
             const wxString& globalFilename = wxString(),
             long style = 0) {}
};
