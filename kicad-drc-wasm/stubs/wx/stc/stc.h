#pragma once
#include "../textctrl.h"
class wxStyledTextCtrl : public wxTextCtrl {
public:
    wxStyledTextCtrl() = default;
    wxStyledTextCtrl(wxWindow*, wxWindowID = wxID_ANY, const wxPoint& = wxPoint(),
                     const wxSize& = wxSize(), long = 0) {}
    void SetText(const wxString&) {}
    wxString GetText() const { return wxString(); }
    void SetReadOnly(bool) {}
    void StyleClearAll() {}
    void SetMarginWidth(int, int) {}
    void SetLexer(int) {}
    void StyleSetForeground(int, const wxColour&) {}
    void StyleSetBackground(int, const wxColour&) {}
    void SetCaretLineVisible(bool) {}
    int GetCurrentPos() const { return 0; }
    int GetCurrentLine() const { return 0; }
    void GotoPos(int) {}
    void GotoLine(int) {}
    int GetLineCount() const { return 0; }
    int GetLength() const { return 0; }
    void AddText(const wxString&) {}
    void ClearAll() {}
    void SetSavePoint() {}
    bool GetModify() const { return false; }
    void EmptyUndoBuffer() {}
    void Undo() {}
    void Redo() {}
    bool CanUndo() const { return false; }
    bool CanRedo() const { return false; }
    void MarkerDefine(int, int, const wxColour& = wxColour(), const wxColour& = wxColour()) {}
    int MarkerAdd(int, int) { return 0; }
    void MarkerDelete(int, int) {}
    void MarkerDeleteAll(int) {}
    void SetIndicatorCurrent(int) {}
    void IndicatorFillRange(int, int) {}
    void IndicatorClearRange(int, int) {}
};

class wxStyledTextEvent : public wxEvent {};

// STC constants
#define wxSTC_STYLE_DEFAULT 32
#define wxSTC_LEX_NULL 1
#define wxSTC_MARGIN_NUMBER 0
#define wxSTC_MARGIN_SYMBOL 1
