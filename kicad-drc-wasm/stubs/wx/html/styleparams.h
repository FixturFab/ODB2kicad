#pragma once
#include "wx/string.h"
#include "wx/arrstr.h"

class wxHtmlTag;

class wxHtmlStyleParams {
public:
    wxHtmlStyleParams(const wxHtmlTag& /*tag*/) {}
    bool HasParam(const wxString& /*par*/) const { return false; }
    wxString GetParam(const wxString& /*par*/) const { return wxString(); }
};
