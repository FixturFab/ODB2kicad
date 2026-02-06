#pragma once
#include "dialog.h"
inline int wxMessageBox(const wxString&, const wxString& = wxString(), int = 0, wxWindow* = nullptr) { return wxOK; }
