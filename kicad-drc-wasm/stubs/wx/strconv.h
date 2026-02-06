#pragma once

#include "string.h"

class wxMBConv
{
public:
    virtual ~wxMBConv() = default;
    virtual size_t MB2WC(wchar_t*, const char*, size_t) const { return 0; }
    virtual size_t WC2MB(char*, const wchar_t*, size_t) const { return 0; }
    wxString cMB2WC(const char* s) const { return wxString(s ? s : ""); }
    wxString cWC2MB(const wchar_t* s) const { return wxString(s); }
};

class wxMBConvUTF8 : public wxMBConv
{
public:
    enum { MAP_INVALID_UTF8_NOT = 0, MAP_INVALID_UTF8_TO_PUA = 1 };
    wxMBConvUTF8(int options = 0) {}
};

class wxCSConv : public wxMBConv
{
public:
    wxCSConv(const wxString&) {}
    wxCSConv(int encoding) {}
};

class wxConvAuto : public wxMBConv
{
public:
    wxConvAuto(int encoding = 0) {}
};

class wxMBConvUTF16LE : public wxMBConv
{
public:
    wxMBConvUTF16LE() = default;
};

// Global converters
extern wxMBConvUTF8 wxConvUTF8;
extern wxCSConv wxConvLocal;
extern wxMBConv wxConvLibc;

// Current conversion (pointer)
inline wxMBConv* wxConvCurrent = &wxConvLibc;
