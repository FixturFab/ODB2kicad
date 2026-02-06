#pragma once
#include "string.h"
#include <cstddef>

inline wxString wxBase64Encode(const void*, size_t) { return wxString(); }
inline wxString wxBase64Encode(const wxString&) { return wxString(); }
inline size_t wxBase64DecodedSize(size_t) { return 0; }
inline size_t wxBase64Decode(void*, size_t, const wxString&) { return 0; }
