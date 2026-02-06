#pragma once
#include "string.h"

class wxDataObject {};
class wxTextDataObject : public wxDataObject {
public:
    wxTextDataObject(const wxString& = wxString()) {}
    wxString GetText() const { return wxString(); }
    void SetText(const wxString&) {}
};

class wxClipboard {
public:
    static wxClipboard* Get() { static wxClipboard cb; return &cb; }
    bool Open() { return true; }
    void Close() {}
    bool SetData(wxDataObject*) { return false; }
    bool GetData(wxDataObject&) { return false; }
    void Clear() {}
    bool IsSupported(int) { return false; }
    bool IsOpened() const { return false; }
};

class wxClipboardLocker {
public:
    wxClipboardLocker(wxClipboard* = nullptr) {}
    operator bool() const { return true; }
};
