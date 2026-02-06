#pragma once
#include "string.h"
class wxMimeTypesManager {
public:
    static wxMimeTypesManager* Get() { static wxMimeTypesManager m; return &m; }
};
class wxFileType {
public:
    bool GetMimeType(wxString*) const { return false; }
    bool GetDescription(wxString*) const { return false; }
};
