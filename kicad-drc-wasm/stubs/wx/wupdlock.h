#pragma once
#include "window.h"
#include "utils.h"
class wxWindowUpdateLocker {
public:
    wxWindowUpdateLocker(wxWindow* = nullptr) {}
};
