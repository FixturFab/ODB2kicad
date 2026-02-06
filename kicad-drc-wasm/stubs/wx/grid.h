#pragma once
#include "window.h"

// Forward declaration - wxGrid is defined below
class wxGrid;

// Grid constants
inline const wxString wxGridNameStr("grid");
#define wxGRID_VALUE_STRING "string"
#define wxGRID_VALUE_NUMBER "long"
#define wxGRID_VALUE_FLOAT "double"
#define wxGRID_VALUE_BOOL "bool"
#define wxGRID_VALUE_CHOICE "choice"

class wxGridCellAttr {
public:
    enum wxAttrKind { Any, Default, Cell, Row, Col, Merged };
    wxGridCellAttr() = default;
    void IncRef() { m_refCount++; }
    void DecRef() { if(--m_refCount <= 0) delete this; }
    wxGridCellAttr* Clone() const { return new wxGridCellAttr(*this); }
    void SetReadOnly(bool = true) {}
    void SetBackgroundColour(const wxColour&) {}
    void SetTextColour(const wxColour&) {}
    void SetFont(const wxFont&) {}
    void SetAlignment(int, int) {}
    void SetRenderer(class wxGridCellRenderer*) {}
    void SetEditor(class wxGridCellEditor*) {}
private:
    int m_refCount = 1;
};

inline void wxSafeDecRef(wxGridCellAttr* attr) { if(attr) attr->DecRef(); }
inline void wxSafeIncRef(wxGridCellAttr* attr) { if(attr) attr->IncRef(); }

class wxGridCellRenderer {
public:
    virtual ~wxGridCellRenderer() = default;
    void IncRef() {}
    void DecRef() { delete this; }
    virtual wxGridCellRenderer* Clone() const { return nullptr; }
    virtual void Draw(wxGrid& grid, wxGridCellAttr& attr, wxDC& dc, const wxRect& rect, int row, int col, bool isSelected);
    virtual wxSize GetBestSize(wxGrid& grid, wxGridCellAttr& attr, wxDC& dc, int row, int col);
};

class wxGridCellStringRenderer : public wxGridCellRenderer {
public:
    wxGridCellStringRenderer() = default;
    wxGridCellRenderer* Clone() const override { return new wxGridCellStringRenderer(); }
};

class wxGridCellEditor {
public:
    virtual ~wxGridCellEditor() = default;
    void IncRef() {}
    void DecRef() { delete this; }
    virtual wxGridCellEditor* Clone() const { return nullptr; }
    virtual void Create(wxWindow*, wxWindowID, wxEvtHandler*) {}
    virtual void BeginEdit(int, int, wxGrid*) {}
    virtual bool EndEdit(int, int, const wxGrid*, const wxString&, wxString*) { return false; }
    virtual void ApplyEdit(int, int, wxGrid*) {}
    virtual void Reset() {}
    virtual wxString GetValue() const { return wxString(); }
    virtual void StartingKey(wxKeyEvent&) {}
    virtual void SetValidator(const class wxValidator&) {}
};

class wxGridCellTextEditor : public wxGridCellEditor {
public:
    wxGridCellTextEditor(size_t maxChars = 0) {}
    wxGridCellEditor* Clone() const override { return new wxGridCellTextEditor(); }
};

class wxGridTableBase {
public:
    virtual ~wxGridTableBase() = default;
    virtual int GetNumberRows() { return 0; }
    virtual int GetNumberCols() { return 0; }
    virtual bool IsEmptyCell(int, int) { return true; }
    virtual wxString GetValue(int, int) { return wxString(); }
    virtual void SetValue(int, int, const wxString&) {}
    virtual wxString GetTypeName(int, int) { return wxString(wxGRID_VALUE_STRING); }
    virtual wxString GetColLabelValue(int) { return wxString(); }
    virtual wxString GetRowLabelValue(int) { return wxString(); }
    virtual wxGridCellAttr* GetAttr(int, int, wxGridCellAttr::wxAttrKind) { return nullptr; }
    virtual bool DeleteRows(size_t pos = 0, size_t numRows = 1) { return false; }
    virtual bool InsertRows(size_t pos = 0, size_t numRows = 1) { return false; }
    virtual bool AppendRows(size_t numRows = 1) { return false; }
    virtual bool CanGetValueAs(int, int, const wxString&) { return false; }
    virtual bool CanSetValueAs(int, int, const wxString&) { return false; }
    virtual long GetValueAsLong(int, int) { return 0; }
    virtual void SetValueAsLong(int, int, long) {}
    virtual void SetColAttr(wxGridCellAttr* attr, int col) { if(attr) attr->DecRef(); }
    virtual void Clear() {}
    class wxGrid* GetView() const { return nullptr; }
};

#define wxGRIDTABLE_NOTIFY_ROWS_APPENDED 1
#define wxGRIDTABLE_NOTIFY_ROWS_DELETED 2
#define wxGRIDTABLE_NOTIFY_COLS_APPENDED 3
#define wxGRIDTABLE_NOTIFY_COLS_DELETED 4

class wxGridTableMessage {
public:
    wxGridTableMessage() = default;
    wxGridTableMessage(wxGridTableBase*, int, int = 0, int = 0) {}
};

class wxGridUpdateLocker {
public:
    wxGridUpdateLocker(class wxGrid* = nullptr) {}
    ~wxGridUpdateLocker() {}
    void Create(class wxGrid*) {}
};

class wxGrid : public wxWindow {
public:
    wxGrid() = default;
    wxGrid(wxWindow*, wxWindowID, const wxPoint& = wxPoint(), const wxSize& = wxSize(),
           long = 0, const wxString& name = wxGridNameStr) {}
    virtual ~wxGrid() = default;

    bool Create(wxWindow*, wxWindowID, const wxPoint& = wxPoint(), const wxSize& = wxSize(),
                long = 0, const wxString& = wxGridNameStr) { return true; }

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
    bool DeleteRows(int = 0, int = 1, bool = true) { return true; }
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
    void SetTable(wxGridTableBase*, bool = false, int = 0) {}
    wxGridTableBase* GetTable() const { return nullptr; }
    bool ProcessTableMessage(wxGridTableMessage&) { return true; }
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

    // Virtual methods that WX_GRID overrides
    virtual wxSize DoGetBestSize() const { return wxSize(100, 100); }
    virtual bool Enable(bool enable = true) { return true; }
    virtual void SetColMinimalWidth(int col, int width) {}
    virtual void DrawCornerLabel(class wxDC&) {}
    virtual void DrawColLabel(class wxDC&, int) {}
    virtual void DrawRowLabel(class wxDC&, int) {}
    virtual void SetColMinimalAcceptableWidth(int width) {}
    void SetLabelFont(const wxFont&) {}
    void EnableAlternateRowColors(bool = true) {}

    // Protected members that WX_GRID accesses
    bool m_waitForSlowClick = false;
};

class wxGridEvent : public wxEvent {
public:
    int GetRow() const { return 0; }
    int GetCol() const { return 0; }
};

// Grid event types
wxDECLARE_EVENT(wxEVT_GRID_CELL_CHANGED, wxGridEvent);
wxDECLARE_EVENT(wxEVT_GRID_CELL_LEFT_CLICK, wxGridEvent);
wxDECLARE_EVENT(wxEVT_GRID_CELL_RIGHT_CLICK, wxGridEvent);
wxDECLARE_EVENT(wxEVT_GRID_LABEL_LEFT_CLICK, wxGridEvent);
wxDECLARE_EVENT(wxEVT_GRID_LABEL_RIGHT_CLICK, wxGridEvent);

// Deferred inline definitions (wxGrid is now complete)
inline void wxGridCellRenderer::Draw(wxGrid&, wxGridCellAttr&, wxDC&, const wxRect&, int, int, bool) {}
inline wxSize wxGridCellRenderer::GetBestSize(wxGrid&, wxGridCellAttr&, wxDC&, int, int) { return wxSize(); }
