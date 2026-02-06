#pragma once
#include "defs.h"
#include "string.h"

class wxClassInfo;

class wxObject
{
public:
    virtual ~wxObject() = default;
    virtual wxClassInfo* GetClassInfo() const { return nullptr; }
    bool IsKindOf(const wxClassInfo*) const { return false; }
};

class wxClassInfo
{
public:
    const char* GetClassName() const { return "wxObject"; }
    static wxClassInfo* FindClass(const wxString&) { return nullptr; }
};

class wxRefCounter
{
public:
    wxRefCounter() : m_count(1) {}
    virtual ~wxRefCounter() = default;
    int GetRefCount() const { return m_count; }
    void IncRef() { m_count++; }
    void DecRef() { if(--m_count == 0) delete this; }
private:
    int m_count;
};

#define wxDECLARE_DYNAMIC_CLASS(cls)
#define wxDECLARE_ABSTRACT_CLASS(cls)
#define wxDECLARE_CLASS(cls)
#define wxIMPLEMENT_DYNAMIC_CLASS(cls, base)
#define wxIMPLEMENT_ABSTRACT_CLASS(cls, base)
#define wxIMPLEMENT_CLASS(cls, base)
#define DECLARE_DYNAMIC_CLASS(cls)
#define IMPLEMENT_DYNAMIC_CLASS(cls, base)
#define wxDECLARE_NO_COPY_CLASS(cls) cls(const cls&) = delete; cls& operator=(const cls&) = delete
