#pragma once

#include "string.h"

class wxTextEntry
{
public:
    virtual ~wxTextEntry() = default;
    virtual wxString GetValue() const { return wxString(); }
    virtual void SetValue(const wxString&) {}
    virtual void SetSelection(long from, long to) {}
    virtual bool IsEditable() const { return false; }
    virtual void WriteText(const wxString&) {}
    virtual void AppendText(const wxString&) {}
    virtual long GetInsertionPoint() const { return 0; }
    virtual void SetInsertionPoint(long) {}
    virtual void SetInsertionPointEnd() {}
    virtual void SelectAll() {}
    virtual bool HasSelection() const { return false; }
    virtual void Copy() {}
    virtual void Cut() {}
    virtual void Paste() {}
    virtual void Undo() {}
    virtual void Redo() {}
    virtual bool CanCopy() const { return false; }
    virtual bool CanCut() const { return false; }
    virtual bool CanPaste() const { return false; }
    virtual bool CanUndo() const { return false; }
    virtual bool CanRedo() const { return false; }
};
