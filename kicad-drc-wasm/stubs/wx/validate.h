#pragma once
#include "event.h"

class wxValidator : public wxEvtHandler {
public:
    wxValidator() = default;
    virtual ~wxValidator() = default;
    virtual wxValidator* Clone() const { return new wxValidator(); }
    virtual bool Validate(class wxWindow*) { return true; }
    virtual bool TransferToWindow() { return true; }
    virtual bool TransferFromWindow() { return true; }
};

extern const wxValidator wxDefaultValidator;
