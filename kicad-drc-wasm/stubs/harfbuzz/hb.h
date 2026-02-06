#pragma once
// Minimal HarfBuzz stub for WASM build (font shaping not needed for DRC)

#include <cstddef>
#include <cstdint>

typedef uint32_t hb_codepoint_t;
typedef int32_t  hb_position_t;
typedef uint32_t hb_mask_t;
typedef uint32_t hb_tag_t;
typedef int hb_bool_t;

// Buffer
typedef struct hb_buffer_t hb_buffer_t;

inline hb_buffer_t* hb_buffer_create() { return nullptr; }
inline void hb_buffer_destroy(hb_buffer_t* buf) {}
inline void hb_buffer_add_utf8(hb_buffer_t* buf, const char* text, int text_length, unsigned int item_offset, int item_length) {}
inline void hb_buffer_guess_segment_properties(hb_buffer_t* buf) {}

// Glyph info and position
typedef struct {
    hb_codepoint_t codepoint;
    hb_mask_t      mask;
    uint32_t       cluster;
    uint32_t       var1;
    uint32_t       var2;
} hb_glyph_info_t;

typedef struct {
    hb_position_t x_advance;
    hb_position_t y_advance;
    hb_position_t x_offset;
    hb_position_t y_offset;
    uint32_t      var;
} hb_glyph_position_t;

inline hb_glyph_info_t* hb_buffer_get_glyph_infos(hb_buffer_t* buf, unsigned int* length) {
    if (length) *length = 0;
    return nullptr;
}

inline hb_glyph_position_t* hb_buffer_get_glyph_positions(hb_buffer_t* buf, unsigned int* length) {
    if (length) *length = 0;
    return nullptr;
}

// Font
typedef struct hb_font_t hb_font_t;

inline void hb_font_destroy(hb_font_t* font) {}
inline void hb_shape(hb_font_t* font, hb_buffer_t* buf, const void* features, unsigned int num_features) {}

// Version
inline const char* hb_version_string() { return "0.0.0-stub"; }

// Version macros
#define HB_VERSION_STRING "0.0.0-stub"
#define HB_VERSION_MAJOR 0
#define HB_VERSION_MINOR 0
#define HB_VERSION_MICRO 0
