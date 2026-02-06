#pragma once
#include "string.h"
#include <any>
#include <cstring>

class wxVariant {
public:
    wxVariant() = default;
    wxVariant(const wxString& s) : m_string(s) {}
    wxVariant(int i) : m_long(i) {}
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

// wxAnyValueBuffer - storage for wxAny values
union wxAnyValueBuffer
{
    void*    m_ptr;
    char     m_buffer[sizeof(void*) > sizeof(double) ? sizeof(void*) : sizeof(double)];
    double   m_double; // for alignment
};

// Forward declare wxAnyValueTypeImpl so CheckType() can reference it
template<typename T> class wxAnyValueTypeImpl;

// wxAnyValueType - base class for type metadata
class wxAnyValueType
{
public:
    virtual ~wxAnyValueType() = default;
    virtual void DeleteValue(wxAnyValueBuffer& buf) const { (void)buf; }
    virtual void CopyBuffer(const wxAnyValueBuffer& src, wxAnyValueBuffer& dst) const { dst = src; }
    virtual bool ConvertValue(const wxAnyValueBuffer& src, wxAnyValueType* dstType, wxAnyValueBuffer& dst) const { (void)src; (void)dstType; (void)dst; return false; }

    template<typename T> bool CheckType() const {
        return dynamic_cast<const wxAnyValueTypeImpl<T>*>(this) != nullptr;
    }

    // Equality check by pointer
    bool IsSameType(const wxAnyValueType* other) const { return this == other; }
};

// wxAnyValueTypeImplBase<T> - template for storing values of type T
template<typename T>
class wxAnyValueTypeImplBase : public wxAnyValueType
{
public:
    static void SetValue(const T& value, wxAnyValueBuffer& buf) {
        // Store small types in buffer directly, larger types on heap
        if constexpr (sizeof(T) <= sizeof(wxAnyValueBuffer)) {
            memcpy(&buf, &value, sizeof(T));
        } else {
            buf.m_ptr = new T(value);
        }
    }
    static T GetValue(const wxAnyValueBuffer& buf) {
        if constexpr (sizeof(T) <= sizeof(wxAnyValueBuffer)) {
            T val;
            memcpy(&val, &buf, sizeof(T));
            return val;
        } else {
            return *static_cast<const T*>(static_cast<const void*>(&buf.m_ptr));
        }
    }
};

// wxAnyValueTypeImpl<T> - concrete template specialization
template<typename T>
class wxAnyValueTypeImpl : public wxAnyValueTypeImplBase<T>
{
public:
    wxAnyValueTypeImpl() = default;
    virtual ~wxAnyValueTypeImpl() = default;
    static wxAnyValueTypeImpl<T>* sm_instance;
    static wxAnyValueType* GetInstance() {
        if (!sm_instance) sm_instance = new wxAnyValueTypeImpl<T>();
        return sm_instance;
    }
};

template<typename T>
wxAnyValueTypeImpl<T>* wxAnyValueTypeImpl<T>::sm_instance = nullptr;

// WX_DECLARE_ANY_VALUE_TYPE - declares GetInstance for specialized types
#define WX_DECLARE_ANY_VALUE_TYPE(T) \
    public: \
        static wxAnyValueType* GetInstance() { \
            static T s_instance; \
            return &s_instance; \
        }

// WX_IMPLEMENT_ANY_VALUE_TYPE - no-op in our stub (instance is created inline)
#define WX_IMPLEMENT_ANY_VALUE_TYPE(T)

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
