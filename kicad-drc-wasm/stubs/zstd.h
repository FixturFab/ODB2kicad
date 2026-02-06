#pragma once
// zstd.h stub for WASM build - compression not available
// embedded_files.cpp will fail gracefully

#include <stddef.h>

#define ZSTD_CONTENTSIZE_ERROR ((unsigned long long)(-2))
#define ZSTD_CONTENTSIZE_UNKNOWN ((unsigned long long)(-1))

inline size_t ZSTD_compressBound(size_t srcSize) { return srcSize + 128; }
inline size_t ZSTD_compress(void* dst, size_t dstCapacity, const void* src, size_t srcSize, int compressionLevel) {
    (void)dst; (void)dstCapacity; (void)src; (void)srcSize; (void)compressionLevel;
    return (size_t)-1; // error
}
inline unsigned ZSTD_isError(size_t code) { return code == (size_t)-1; }
inline const char* ZSTD_getErrorName(size_t code) { (void)code; return "zstd not available in WASM build"; }
inline unsigned long long ZSTD_getFrameContentSize(const void* src, size_t srcSize) {
    (void)src; (void)srcSize;
    return ZSTD_CONTENTSIZE_ERROR;
}
inline size_t ZSTD_decompress(void* dst, size_t dstCapacity, const void* src, size_t srcSize) {
    (void)dst; (void)dstCapacity; (void)src; (void)srcSize;
    return (size_t)-1;
}
