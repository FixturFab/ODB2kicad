#pragma once
#include "window.h"

class wxGLContext {
public:
    wxGLContext(wxWindow* = nullptr) {}
    bool SetCurrent(const class wxGLCanvas&) { return true; }
};

class wxGLCanvas : public wxWindow {
public:
    wxGLCanvas() = default;
    wxGLCanvas(wxWindow*, wxWindowID = wxID_ANY, const int* = nullptr,
               const wxPoint& = wxPoint(), const wxSize& = wxSize(), long = 0) {}
    bool SetCurrent(const wxGLContext&) { return true; }
    void SwapBuffers() {}
};

class wxGLAttributes {
public:
    wxGLAttributes& RGBA() { return *this; }
    wxGLAttributes& DoubleBuffer() { return *this; }
    wxGLAttributes& Depth(int) { return *this; }
    wxGLAttributes& Stencil(int) { return *this; }
    wxGLAttributes& EndList() { return *this; }
    wxGLAttributes& PlatformDefaults() { return *this; }
    wxGLAttributes& MinRGBA(int, int, int, int) { return *this; }
    wxGLAttributes& SampleBuffers(int) { return *this; }
    wxGLAttributes& Samplers(int) { return *this; }
    const int* GetGLAttrs() const { return nullptr; }
};

class wxGLContextAttrs {
public:
    wxGLContextAttrs& CoreProfile() { return *this; }
    wxGLContextAttrs& MajorVersion(int) { return *this; }
    wxGLContextAttrs& MinorVersion(int) { return *this; }
    wxGLContextAttrs& EndList() { return *this; }
    wxGLContextAttrs& OGLVersion(int, int) { return *this; }
    wxGLContextAttrs& CompatibilityProfile() { return *this; }
};
