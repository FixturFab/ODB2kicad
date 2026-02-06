#pragma once
#include "stream.h"
class wxZlibInputStream : public wxInputStream {
public:
    wxZlibInputStream(wxInputStream&) {}
};
class wxZlibOutputStream : public wxOutputStream {
public:
    wxZlibOutputStream(wxOutputStream&, int = -1) {}
};
