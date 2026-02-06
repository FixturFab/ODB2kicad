#pragma once
// Minimal fontconfig stub for WASM build
// DRC doesn't need font rendering, but the fontconfig.cpp is compiled as part of kicommon

#include <cstdint>

typedef int FcBool;
typedef unsigned char FcChar8;
typedef uint32_t FcChar32;

#define FcTrue 1
#define FcFalse 0

#define FC_FAMILY "family"
#define FC_STYLE "style"
#define FC_FILE "file"
#define FC_INDEX "index"
#define FC_WEIGHT "weight"
#define FC_SLANT "slant"
#define FC_WIDTH "width"
#define FC_LANG "lang"
#define FC_FAMILYLANG "familylang"
#define FC_OUTLINE "outline"

#define FC_MAJOR 2
#define FC_MINOR 14
#define FC_REVISION 0

#define FC_WEIGHT_REGULAR 80
#define FC_WEIGHT_BOLD 200
#define FC_SLANT_ROMAN 0
#define FC_SLANT_ITALIC 100

typedef enum {
    FcResultMatch,
    FcResultNoMatch,
    FcResultTypeMismatch,
    FcResultNoId
} FcResult;

typedef enum {
    FcMatchPattern,
    FcMatchFont,
    FcMatchScan
} FcMatchKind;

struct FcConfig;
struct FcPattern;
struct FcObjectSet;
struct FcFontSet {
    int nfont;
    int sfont;
    FcPattern** fonts;
};
struct FcCharSet;
struct FcLangSet;
struct FcValue;

inline FcBool FcInit() { return FcTrue; }

inline FcConfig* FcInitLoadConfigAndFonts() { return nullptr; }
inline FcBool FcConfigSetCurrent(FcConfig*) { return FcFalse; }
inline FcConfig* FcConfigGetCurrent() { return nullptr; }
inline void FcConfigDestroy(FcConfig*) {}
inline FcBool FcConfigAppFontAddFile(FcConfig*, const FcChar8*) { return FcFalse; }

inline FcPattern* FcPatternCreate() { return nullptr; }
inline void FcPatternDestroy(FcPattern*) {}
inline FcBool FcPatternAddString(FcPattern*, const char*, const FcChar8*) { return FcFalse; }
inline FcBool FcPatternAddInteger(FcPattern*, const char*, int) { return FcFalse; }
inline FcBool FcPatternAddBool(FcPattern*, const char*, FcBool) { return FcFalse; }
inline FcResult FcPatternGetString(FcPattern*, const char*, int, FcChar8**) { return FcResultNoMatch; }
inline FcResult FcPatternGetInteger(FcPattern*, const char*, int, int*) { return FcResultNoMatch; }
inline FcResult FcPatternGetBool(FcPattern*, const char*, int, FcBool*) { return FcResultNoMatch; }
inline FcResult FcPatternGetLangSet(FcPattern*, const char*, int, FcLangSet**) { return FcResultNoMatch; }

inline FcObjectSet* FcObjectSetBuild(const char*, ...) { return nullptr; }
inline void FcObjectSetDestroy(FcObjectSet*) {}

inline FcFontSet* FcFontList(FcConfig*, FcPattern*, FcObjectSet*) { return nullptr; }
inline void FcFontSetDestroy(FcFontSet*) {}

inline FcBool FcConfigSubstitute(FcConfig*, FcPattern*, FcMatchKind) { return FcFalse; }
inline void FcDefaultSubstitute(FcPattern*) {}
inline FcPattern* FcFontMatch(FcConfig*, FcPattern*, FcResult*) { return nullptr; }

inline FcLangSet* FcLangSetCreate() { return nullptr; }
inline void FcLangSetDestroy(FcLangSet*) {}
inline FcChar8* FcStrSetNext(void*) { return nullptr; }

inline const FcChar8* FcGetVersion_str() { return (const FcChar8*)"stub"; }

// FcFontSet is defined above as a complete type
