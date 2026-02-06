#pragma once
// Minimal HarfBuzz-FreeType bridge stub
#include "hb.h"
#include "freetype2/ft2build.h"

inline hb_font_t* hb_ft_font_create_referenced(FT_Face face) { return nullptr; }
