#pragma once

#include "string.h"

// Translation stubs - no translations in WASM build
#define wxTRANSLATE(s) s
#define wxGetTranslation(s, ...) (s)

class wxTranslations
{
public:
    static wxTranslations* Get() { return nullptr; }
    static void Set(wxTranslations*) {}
    void SetLanguage(const wxString&) {}
    bool AddCatalog(const wxString&) { return true; }
};

class wxLocale
{
public:
    wxLocale() = default;
    wxLocale(int language, int flags = 0) {}
    ~wxLocale() = default;
    bool Init(int language = 0, int flags = 0) { return true; }
    bool IsOk() const { return true; }
    wxString GetName() const { return wxString("C"); }
    wxString GetCanonicalName() const { return wxString("en_US"); }
    static wxString GetLanguageName(int lang) { return wxString("English"); }
    static wxString GetLanguageCanonicalName(int lang) { return wxString("en_US"); }
    void AddCatalogLookupPathPrefix(const wxString&) {}
    bool AddCatalog(const wxString&) { return true; }
    const wxString& GetDecimalPoint() const { static wxString s("."); return s; }
};

// Language IDs
enum {
    wxLANGUAGE_DEFAULT = 0,
    wxLANGUAGE_ENGLISH = 1,
    wxLANGUAGE_ENGLISH_US = 2
};
