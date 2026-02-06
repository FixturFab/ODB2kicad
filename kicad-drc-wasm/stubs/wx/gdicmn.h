#pragma once

#include "defs.h"
#include "string.h"

class wxSize
{
public:
    wxSize() : m_w(0), m_h(0) {}
    wxSize(int w, int h) : m_w(w), m_h(h) {}
    int GetWidth() const { return m_w; }
    int GetHeight() const { return m_h; }
    void SetWidth(int w) { m_w = w; }
    void SetHeight(int h) { m_h = h; }
    int x = 0, y = 0;
    bool operator==(const wxSize& o) const { return m_w == o.m_w && m_h == o.m_h; }
    bool operator!=(const wxSize& o) const { return !(*this == o); }
private:
    int m_w, m_h;
};

class wxPoint
{
public:
    int x, y;
    wxPoint() : x(0), y(0) {}
    wxPoint(int x_, int y_) : x(x_), y(y_) {}
    bool operator==(const wxPoint& o) const { return x == o.x && y == o.y; }
    bool operator!=(const wxPoint& o) const { return !(*this == o); }
    wxPoint operator+(const wxPoint& o) const { return wxPoint(x + o.x, y + o.y); }
    wxPoint operator-(const wxPoint& o) const { return wxPoint(x - o.x, y - o.y); }
};

class wxRect
{
public:
    int x, y, width, height;
    wxRect() : x(0), y(0), width(0), height(0) {}
    wxRect(int x_, int y_, int w, int h) : x(x_), y(y_), width(w), height(h) {}
    wxRect(const wxPoint& p, const wxSize& s) : x(p.x), y(p.y), width(s.GetWidth()), height(s.GetHeight()) {}
    wxRect(const wxSize& s) : x(0), y(0), width(s.GetWidth()), height(s.GetHeight()) {}

    int GetLeft() const { return x; }
    int GetTop() const { return y; }
    int GetRight() const { return x + width - 1; }
    int GetBottom() const { return y + height - 1; }
    int GetWidth() const { return width; }
    int GetHeight() const { return height; }
    wxPoint GetPosition() const { return wxPoint(x, y); }
    wxSize GetSize() const { return wxSize(width, height); }

    void SetWidth(int w) { width = w; }
    void SetHeight(int h) { height = h; }
    void SetX(int x_) { x = x_; }
    void SetY(int y_) { y = y_; }
    void SetPosition(const wxPoint& p) { x = p.x; y = p.y; }
    void SetSize(const wxSize& s) { width = s.GetWidth(); height = s.GetHeight(); }

    bool Contains(int x_, int y_) const {
        return x_ >= x && x_ < x + width && y_ >= y && y_ < y + height;
    }
    bool Contains(const wxPoint& p) const { return Contains(p.x, p.y); }
    bool Intersects(const wxRect& r) const {
        return !(r.x >= x + width || r.x + r.width <= x || r.y >= y + height || r.y + r.height <= y);
    }

    wxRect Union(const wxRect& r) const {
        int l = std::min(x, r.x);
        int t = std::min(y, r.y);
        int right = std::max(x + width, r.x + r.width);
        int bottom = std::max(y + height, r.y + r.height);
        return wxRect(l, t, right - l, bottom - t);
    }

    wxRect Inflate(int dx, int dy) const {
        return wxRect(x - dx, y - dy, width + 2*dx, height + 2*dy);
    }

    bool IsEmpty() const { return width <= 0 || height <= 0; }
    bool operator==(const wxRect& o) const {
        return x == o.x && y == o.y && width == o.width && height == o.height;
    }
};

class wxRealPoint
{
public:
    double x, y;
    wxRealPoint() : x(0), y(0) {}
    wxRealPoint(double x_, double y_) : x(x_), y(y_) {}
};

// Cursor stub
class wxCursor
{
public:
    wxCursor() = default;
    wxCursor(int cursorId) {}
};

// Colour constants
#define wxALPHA_OPAQUE 255
#define wxALPHA_TRANSPARENT 0

// Colour base class
class wxColourBase {
public:
    typedef unsigned char ChannelType;
};

// Colour stub
class wxColour : public wxColourBase
{
public:
    wxColour() : m_r(0), m_g(0), m_b(0), m_a(255) {}
    wxColour(unsigned char r, unsigned char g, unsigned char b, unsigned char a = wxALPHA_OPAQUE)
        : m_r(r), m_g(g), m_b(b), m_a(a) {}
    wxColour(const wxString& name) : m_r(0), m_g(0), m_b(0), m_a(255) {}
    wxColour(unsigned long colRGB) {
        m_r = (unsigned char)(colRGB & 0xFF);
        m_g = (unsigned char)((colRGB >> 8) & 0xFF);
        m_b = (unsigned char)((colRGB >> 16) & 0xFF);
        m_a = 255;
    }

    unsigned char Red() const { return m_r; }
    unsigned char Green() const { return m_g; }
    unsigned char Blue() const { return m_b; }
    unsigned char Alpha() const { return m_a; }
    bool IsOk() const { return true; }

    void Set(unsigned char r, unsigned char g, unsigned char b, unsigned char a = wxALPHA_OPAQUE) {
        m_r = r; m_g = g; m_b = b; m_a = a;
    }
    void Set(unsigned long colRGB) {
        m_r = (unsigned char)(colRGB & 0xFF);
        m_g = (unsigned char)((colRGB >> 8) & 0xFF);
        m_b = (unsigned char)((colRGB >> 16) & 0xFF);
        m_a = 255;
    }
    bool Set(const wxString&) { return false; }

    bool operator==(const wxColour& o) const {
        return m_r == o.m_r && m_g == o.m_g && m_b == o.m_b && m_a == o.m_a;
    }
    bool operator!=(const wxColour& o) const { return !(*this == o); }
    wxString GetAsString(long flags = 0) const { return wxString(); }

private:
    unsigned char m_r, m_g, m_b, m_a;
};

// wxColor is an alias for wxColour (American spelling)
#define wxColor wxColour

// Bitmap type enum
enum wxBitmapType
{
    wxBITMAP_TYPE_INVALID = 0,
    wxBITMAP_TYPE_BMP,
    wxBITMAP_TYPE_ICO,
    wxBITMAP_TYPE_CUR,
    wxBITMAP_TYPE_XBM,
    wxBITMAP_TYPE_XPM,
    wxBITMAP_TYPE_TIFF,
    wxBITMAP_TYPE_GIF,
    wxBITMAP_TYPE_PNG,
    wxBITMAP_TYPE_JPEG,
    wxBITMAP_TYPE_PNM,
    wxBITMAP_TYPE_PCX,
    wxBITMAP_TYPE_PICT,
    wxBITMAP_TYPE_ICON,
    wxBITMAP_TYPE_ANI,
    wxBITMAP_TYPE_IFF,
    wxBITMAP_TYPE_TGA,
    wxBITMAP_TYPE_MACCURSOR,
    wxBITMAP_TYPE_ANY = 50
};

// Bitmap stub
class wxBitmap
{
public:
    wxBitmap() = default;
    wxBitmap(int w, int h, int depth = -1) {}
    wxBitmap(const wxString& name, int type = 0) {}
    bool IsOk() const { return false; }
    int GetWidth() const { return 0; }
    int GetHeight() const { return 0; }
};

class wxImage
{
public:
    wxImage() = default;
    wxImage(int w, int h) {}
    bool IsOk() const { return false; }
    int GetWidth() const { return 0; }
    int GetHeight() const { return 0; }
    static void AddHandler(void*) {}
    unsigned char* GetData() const { return nullptr; }
    bool HasAlpha() const { return false; }
    unsigned char* GetAlpha() const { return nullptr; }
};

class wxIcon : public wxBitmap
{
public:
    wxIcon() = default;
    void CopyFromBitmap(const wxBitmap&) {}
};

// wxStaticBitmap forward-declared here, defined after wxWindow is complete
class wxStaticBitmap;

// Pen/Brush stubs
class wxPen
{
public:
    wxPen() = default;
    wxPen(const wxColour&, int width = 1, int style = 0) {}
};

class wxBrush
{
public:
    wxBrush() = default;
    wxBrush(const wxColour&, int style = 0) {}
};

class wxFont
{
public:
    wxFont() = default;
    wxFont(int size, int family, int style, int weight) {}
    wxFont(const wxSize&, int family, int style, int weight) {}
    bool IsOk() const { return false; }
    int GetPointSize() const { return 10; }
    wxString GetFaceName() const { return wxString(); }
    void SetPointSize(int) {}
};

// Default position and size constants
extern const wxPoint wxDefaultPosition;
extern const wxSize wxDefaultSize;
