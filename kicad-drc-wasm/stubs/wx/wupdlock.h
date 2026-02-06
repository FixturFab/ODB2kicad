#pragma once
#include "window.h"
class wxWindowUpdateLocker {
public:
    wxWindowUpdateLocker(wxWindow* = nullptr) {}
};
