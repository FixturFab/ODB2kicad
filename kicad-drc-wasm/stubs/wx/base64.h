#pragma once
#include "string.h"
#include <cstddef>
#include <cstring>

inline size_t wxBase64EncodedSize(size_t len) { return (4 * (len + 2) / 3) + 1; }

inline size_t wxBase64Encode(char* dst, size_t dstLen, const void* src, size_t srcLen) {
    // Minimal base64 encoder
    static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t needed = wxBase64EncodedSize(srcLen);
    if (!dst || dstLen < needed) return needed;
    const unsigned char* s = (const unsigned char*)src;
    size_t o = 0;
    for (size_t i = 0; i < srcLen; i += 3) {
        unsigned int n = ((unsigned int)s[i]) << 16;
        if (i + 1 < srcLen) n |= ((unsigned int)s[i + 1]) << 8;
        if (i + 2 < srcLen) n |= (unsigned int)s[i + 2];
        dst[o++] = b64[(n >> 18) & 0x3F];
        dst[o++] = b64[(n >> 12) & 0x3F];
        dst[o++] = (i + 1 < srcLen) ? b64[(n >> 6) & 0x3F] : '=';
        dst[o++] = (i + 2 < srcLen) ? b64[n & 0x3F] : '=';
    }
    dst[o] = '\0';
    return o;
}

inline wxString wxBase64Encode(const void* src, size_t srcLen) {
    size_t needed = wxBase64EncodedSize(srcLen);
    char* buf = new char[needed + 1];
    size_t len = wxBase64Encode(buf, needed + 1, src, srcLen);
    wxString result(buf, len);
    delete[] buf;
    return result;
}
inline wxString wxBase64Encode(const wxString&) { return wxString(); }
inline size_t wxBase64DecodedSize(size_t) { return 0; }
inline size_t wxBase64Decode(void*, size_t, const wxString&) { return 0; }
