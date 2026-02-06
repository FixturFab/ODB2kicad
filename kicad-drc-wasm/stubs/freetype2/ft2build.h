#pragma once
// Minimal FreeType stub for WASM build

#define FT_FREETYPE_H "freetype2/freetype.h"

typedef int FT_Int;
typedef int FT_Error;
typedef void* FT_Library;
typedef void* FT_Face;

inline FT_Error FT_Init_FreeType(FT_Library* lib) { *lib = nullptr; return 0; }
inline void FT_Library_Version(FT_Library lib, FT_Int* major, FT_Int* minor, FT_Int* patch) {
    if(major) *major = 0; if(minor) *minor = 0; if(patch) *patch = 0;
}
inline FT_Error FT_Done_FreeType(FT_Library lib) { return 0; }
