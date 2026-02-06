#pragma once
#include "dialog.h"
class wxFileDialog : public wxDialog {
public:
    wxFileDialog(wxWindow* = nullptr, const wxString& = wxString(), const wxString& = wxString(),
                 const wxString& = wxString(), const wxString& = wxString(), long = 0) {}
    wxString GetPath() const { return wxString(); }
    wxString GetFilename() const { return wxString(); }
    wxString GetDirectory() const { return wxString(); }
    int GetFilterIndex() const { return 0; }
    void SetFilterIndex(int) {}
};
#define wxFD_OPEN 0
#define wxFD_SAVE 1
#define wxFD_OVERWRITE_PROMPT 2
#define wxFD_FILE_MUST_EXIST 4
#define wxFD_MULTIPLE 8
