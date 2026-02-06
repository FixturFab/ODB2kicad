#pragma once
#include "string.h"
class wxSingleInstanceChecker {
public:
    wxSingleInstanceChecker() = default;
    wxSingleInstanceChecker(const wxString&, const wxString& = wxString()) {}
    bool Create(const wxString&, const wxString& = wxString()) { return true; }
    bool IsAnotherRunning() const { return false; }
};
