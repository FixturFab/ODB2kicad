#pragma once
#include "string.h"
class wxMessageOutput {
public:
    static wxMessageOutput* Get() { return nullptr; }
    static wxMessageOutput* Set(wxMessageOutput*) { return nullptr; }
    virtual void Printf(const wxString&, ...) {}
};
class wxMessageOutputStderr : public wxMessageOutput {};
