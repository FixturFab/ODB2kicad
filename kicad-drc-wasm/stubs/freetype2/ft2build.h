#pragma once
// Comprehensive FreeType stub for WASM build
// Provides all types/functions needed by KiCad font subsystem (non-functional stubs)

#include <cstddef>

// FT header macros (used as #include FT_FREETYPE_H, etc.)
#define FT_FREETYPE_H       "freetype2/freetype.h"
#define FT_OUTLINE_H        "freetype2/ftoutln.h"
#define FT_SFNT_NAMES_H     "freetype2/ftsnames.h"
#define FT_TRUETYPE_TABLES_H "freetype2/tttables.h"
#define FT_GLYPH_H          "freetype2/ftglyph.h"
#define FT_BBOX_H           "freetype2/ftbbox.h"

// Primitive types
typedef int            FT_Int;
typedef unsigned int   FT_UInt;
typedef long           FT_Long;
typedef unsigned long  FT_ULong;
typedef short          FT_Short;
typedef unsigned short FT_UShort;
typedef signed char    FT_Char;
typedef unsigned char  FT_Byte;
typedef int            FT_Error;
typedef long           FT_Fixed;
typedef long           FT_Pos;
typedef int            FT_Int32;
typedef unsigned int   FT_UInt32;

// FT_Vector
typedef struct FT_Vector_ {
    FT_Pos x;
    FT_Pos y;
} FT_Vector;

// FT_BBox
typedef struct FT_BBox_ {
    FT_Pos xMin, yMin;
    FT_Pos xMax, yMax;
} FT_BBox;

// FT_Matrix
typedef struct FT_Matrix_ {
    FT_Fixed xx, xy;
    FT_Fixed yx, yy;
} FT_Matrix;

// FT_Bitmap
typedef struct FT_Bitmap_ {
    unsigned int rows;
    unsigned int width;
    int          pitch;
    unsigned char* buffer;
    unsigned short num_grays;
    unsigned char  pixel_mode;
    unsigned char  palette_mode;
    void*          palette;
} FT_Bitmap;

// FT_Outline
typedef struct FT_Outline_ {
    short        n_contours;
    short        n_points;
    FT_Vector*   points;
    char*        tags;
    short*       contours;
    int          flags;
} FT_Outline;

// FT_GlyphSlotRec
typedef struct FT_GlyphSlotRec_ {
    void*       library;
    void*       face;
    void*       next;
    FT_UInt     glyph_index;
    long        linearHoriAdvance;
    long        linearVertAdvance;
    FT_Vector   advance;
    int         format;
    FT_Bitmap   bitmap;
    int         bitmap_left;
    int         bitmap_top;
    FT_Outline  outline;
    FT_UInt     num_subglyphs;
    void*       subglyphs;
    void*       control_data;
    long        control_len;
    FT_Pos      lsb_delta;
    FT_Pos      rsb_delta;
    void*       other;
    void*       internal_;
} FT_GlyphSlotRec;
typedef FT_GlyphSlotRec* FT_GlyphSlot;

// FT_Size_Metrics
typedef struct FT_Size_Metrics_ {
    FT_UShort x_ppem;
    FT_UShort y_ppem;
    FT_Fixed  x_scale;
    FT_Fixed  y_scale;
    FT_Pos    ascender;
    FT_Pos    descender;
    FT_Pos    height;
    FT_Pos    max_advance;
} FT_Size_Metrics;

typedef struct FT_SizeRec_ {
    void*           face;
    void*           generic_data;
    void*           generic_finalizer;
    FT_Size_Metrics metrics;
    void*           internal_;
} FT_SizeRec;
typedef FT_SizeRec* FT_Size;

// FT_FaceRec (must be a struct so ->style_flags works)
typedef struct FT_FaceRec_ {
    FT_Long        num_faces;
    FT_Long        face_index;
    FT_Long        face_flags;
    FT_Long        style_flags;
    FT_Long        num_glyphs;
    char*          family_name;
    char*          style_name;
    int            num_fixed_sizes;
    void*          available_sizes;
    int            num_charmaps;
    void*          charmaps;
    void*          generic_data;
    void*          generic_finalizer;
    FT_BBox        bbox;
    FT_UShort      units_per_EM;
    FT_Short       ascender;
    FT_Short       descender;
    FT_Short       height;
    FT_Short       max_advance_width;
    FT_Short       max_advance_height;
    FT_Short       underline_position;
    FT_Short       underline_thickness;
    FT_GlyphSlot   glyph;
    FT_Size         size;
    void*          charmap;
    void*          driver;
    void*          memory;
    void*          stream;
    void*          sizes_list_head;
    void*          sizes_list_tail;
    void*          autohint;
    void*          extensions;
    void*          internal_;
} FT_FaceRec;
typedef FT_FaceRec* FT_Face;

// FT_Library
typedef void* FT_Library;

// Enums/constants
#define FT_STYLE_FLAG_BOLD   (1 << 0)
#define FT_STYLE_FLAG_ITALIC (1 << 1)

#define FT_LOAD_DEFAULT       0x0
#define FT_LOAD_NO_BITMAP     (1 << 3)

#define FT_SFNT_OS2  4

typedef enum FT_Orientation_ {
    FT_ORIENTATION_TRUETYPE   = 0,
    FT_ORIENTATION_POSTSCRIPT = 1,
    FT_ORIENTATION_NONE
} FT_Orientation;

// FT_Encoding
typedef enum FT_Encoding_ {
    FT_ENCODING_NONE      = 0,
    FT_ENCODING_UNICODE   = 0x756E6963  /* 'unic' */
} FT_Encoding_;
typedef FT_Encoding_ FT_Encoding;

// FT_Glyph
typedef struct FT_GlyphRec_ {
    void*    library;
    void*    clazz;
    int      format;
    FT_Vector advance;
} FT_GlyphRec;
typedef FT_GlyphRec* FT_Glyph;

// FT_Outline_Funcs
typedef int (*FT_Outline_MoveToFunc)(const FT_Vector* to, void* user);
typedef int (*FT_Outline_LineToFunc)(const FT_Vector* to, void* user);
typedef int (*FT_Outline_ConicToFunc)(const FT_Vector* control, const FT_Vector* to, void* user);
typedef int (*FT_Outline_CubicToFunc)(const FT_Vector* control1, const FT_Vector* control2, const FT_Vector* to, void* user);

typedef struct FT_Outline_Funcs_ {
    FT_Outline_MoveToFunc  move_to;
    FT_Outline_LineToFunc  line_to;
    FT_Outline_ConicToFunc conic_to;
    FT_Outline_CubicToFunc cubic_to;
    int                    shift;
    FT_Pos                 delta;
} FT_Outline_Funcs;

// TT_OS2 (TrueType OS/2 table)
typedef struct TT_OS2_ {
    FT_UShort version;
    FT_Short  xAvgCharWidth;
    FT_UShort usWeightClass;
    FT_UShort usWidthClass;
    FT_UShort fsType;
} TT_OS2;

#define FT_FSTYPE_INSTALLABLE_EMBEDDING    0x0000
#define FT_FSTYPE_BITMAP_EMBEDDING_ONLY    0x0200
#define FT_FSTYPE_EDITABLE_EMBEDDING       0x0008
#define FT_FSTYPE_PREVIEW_AND_PRINT_EMBEDDING 0x0004

// Stub functions
inline FT_Error FT_Init_FreeType(FT_Library* lib) { *lib = nullptr; return 0; }
inline void FT_Library_Version(FT_Library lib, FT_Int* major, FT_Int* minor, FT_Int* patch) {
    if(major) *major = 0; if(minor) *minor = 0; if(patch) *patch = 0;
}
inline FT_Error FT_Done_FreeType(FT_Library lib) { return 0; }

inline FT_Error FT_New_Face(FT_Library library, const char* filepathname, FT_Long face_index, FT_Face* aface) {
    *aface = nullptr;
    return 1; // error - no font loading in WASM stub
}

inline FT_Error FT_Select_Charmap(FT_Face face, FT_Encoding encoding) { return 0; }
inline FT_Error FT_Set_Char_Size(FT_Face face, FT_Long char_width, FT_Long char_height, FT_UInt horz_resolution, FT_UInt vert_resolution) { return 0; }
inline void*    FT_Get_Sfnt_Table(FT_Face face, int tag) { return nullptr; }
inline FT_Error FT_Load_Glyph(FT_Face face, FT_UInt glyph_index, FT_Int32 load_flags) { return 1; }
inline void     FT_Set_Transform(FT_Face face, FT_Matrix* matrix, FT_Vector* delta) {}
inline FT_Error FT_Outline_Embolden(FT_Outline* outline, FT_Pos strength) { return 0; }
inline FT_Error FT_Outline_Decompose(FT_Outline* outline, const FT_Outline_Funcs* func_interface, void* user) { return 0; }
inline FT_Orientation FT_Outline_Get_Orientation(FT_Outline* outline) { return FT_ORIENTATION_NONE; }
inline FT_Error FT_Get_Glyph(FT_GlyphSlot slot, FT_Glyph* aglyph) { return 1; }
inline FT_UInt  FT_Get_Char_Index(FT_Face face, FT_ULong charcode) { return 0; }
inline FT_Error FT_Done_Face(FT_Face face) { return 0; }

inline FT_Error FT_Outline_Get_BBox(FT_Outline* outline, FT_BBox* abbox) {
    if (abbox) { abbox->xMin = 0; abbox->yMin = 0; abbox->xMax = 0; abbox->yMax = 0; }
    return 0;
}
