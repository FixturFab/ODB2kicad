#pragma once
#include "string.h"
#include <any>

class wxVariant {
public:
    wxVariant() = default;
    wxVariant(const wxString& s) : m_string(s) {}
    wxVariant(long l) : m_long(l) {}
    wxVariant(bool b) : m_long(b ? 1 : 0) {}
    wxVariant(double d) : m_double(d) {}

    wxString GetString() const { return m_string; }
    long GetLong() const { return m_long; }
    bool GetBool() const { return m_long != 0; }
    double GetDouble() const { return m_double; }
    wxString GetType() const { return wxString("string"); }
    long GetInteger() const { return m_long; }
    bool IsNull() const { return false; }

    operator wxString() const { return m_string; }

    wxVariant(class wxVariantData* data) : m_data(data) {}
    class wxVariantData* GetData() const { return m_data; }

private:
    class wxVariantData* m_data = nullptr;
    wxString m_string;
    long m_long = 0;
    double m_double = 0;
};

class wxVariantData
{
public:
    virtual ~wxVariantData() = default;
    virtual bool Eq(wxVariantData&) const { return false; }
    virtual wxString GetType() const { return wxString(); }
    virtual wxVariantData* Clone() const { return nullptr; }
    virtual bool Read(wxString&) { return false; }
    virtual bool Write(wxString&) const { return false; }
    virtual bool GetAsAny(class wxAny*) const { return false; }
    void IncRef() {}
    void DecRef() { delete this; }
};

class wxAny
{
public:
    wxAny() = default;
    template<typename T> wxAny(const T&) {}
    template<typename T> bool GetAs(T* val) const { return false; }
    template<typename T> T As() const { return T(); }
    template<typename T> bool CheckType() const { return false; }
    template<typename T> T RawAs() const { return T(); }
    bool IsNull() const { return true; }
    bool HasSameType(const wxAny&) const { return false; }
};

#define wxANY_AS(any, T) (any).As<T>()
