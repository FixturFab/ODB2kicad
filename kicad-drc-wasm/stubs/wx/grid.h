#pragma once
#include "window.h"
class wxGridCellAttr {};
class wxGridCellRenderer {};
class wxGridCellEditor {};
class wxGrid : public wxWindow {
public:
    wxGrid() = default;
    wxGrid(wxWindow*, wxWindowID, const wxPoint& = wxPoint(), const wxSize& = wxSize(), long = 0) {}
    void CreateGrid(int, int) {}
    int GetNumberRows() const { return 0; }
    int GetNumberCols() const { return 0; }
    wxString GetCellValue(int, int) const { return wxString(); }
    void SetCellValue(int, int, const wxString&) {}
    void SetColLabelValue(int, const wxString&) {}
    void SetRowLabelValue(int, const wxString&) {}
    void AutoSizeColumns(bool = true) {}
    void AutoSizeRows(bool = true) {}
    bool AppendRows(int = 1) { return true; }
    bool AppendCols(int = 1) { return true; }
    bool DeleteRows(int = 0, int = 1) { return true; }
    bool DeleteCols(int = 0, int = 1) { return true; }
    void ClearGrid() {}
    int GetGridCursorRow() const { return 0; }
    int GetGridCursorCol() const { return 0; }
    void SetGridCursor(int, int) {}
    void BeginBatch() {}
    void EndBatch() {}
    void ForceRefresh() {}
    void EnableEditing(bool = true) {}
    void DisableDragGridSize() {}
    void DisableDragRowSize() {}
    void DisableDragColSize() {}
    void SetDefaultCellAlignment(int, int) {}
    void SetColSize(int, int) {}
    void SetRowSize(int, int) {}
    int GetColSize(int) const { return 100; }
    void HideRowLabels() {}
    void SetColLabelSize(int) {}
    void SetRowLabelSize(int) {}
    void SetSelectionMode(int) {}
    void SetTable(void*, bool = false, int = 0) {}
    void SetCellRenderer(int, int, wxGridCellRenderer*) {}
    void SetCellEditor(int, int, wxGridCellEditor*) {}
    void SetReadOnly(int, int, bool = true) {}
    void SetCellBackgroundColour(int, int, const wxColour&) {}
    void SetCellTextColour(int, int, const wxColour&) {}
    void SetCellFont(int, int, const wxFont&) {}
    bool IsSelection() const { return false; }
    wxArrayString GetSelectedRows() const { return wxArrayString(); }
    void SelectRow(int, bool = false) {}
    void ClearSelection() {}
    void MakeCellVisible(int, int) {}
    void CommitPendingChanges(bool = true) {}
    enum { wxGridSelectRows = 0, wxGridSelectCells = 1, wxGridSelectColumns = 2 };
};
class wxGridEvent : public wxEvent {
public:
    int GetRow() const { return 0; }
    int GetCol() const { return 0; }
};
class wxGridTableBase {};
