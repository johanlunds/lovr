#include "loaders/font.h"
#include "util.h"
#include "graphics/texture.h"
#include "math/math.h"
#include "lib/map/map.h"
#include "lib/vec/vec.h"
#include <stdint.h>

#pragma once

typedef enum {
  ALIGN_LEFT,
  ALIGN_RIGHT,
  ALIGN_CENTER
} HorizontalAlign;

typedef enum {
  ALIGN_TOP,
  ALIGN_BOTTOM,
  ALIGN_MIDDLE
} VerticalAlign;

typedef struct {
  int x;
  int y;
  int width;
  int height;
  int rowHeight;
  int padding;
  map_glyph_t glyphs;
} FontAtlas;

typedef struct {
  Ref ref;
  FontData* fontData;
  Texture* texture;
  FontAtlas atlas;
  map_int_t kerning;
  vec_float_t vertices;
  float lineHeight;
} Font;

Font* lovrFontCreate(FontData* fontData);
void lovrFontDestroy(const Ref* ref);
void lovrFontPrint(Font* font, const char* str, mat4 transform, float wrap, HorizontalAlign halign, VerticalAlign valign);
float lovrFontGetLineHeight(Font* font);
void lovrFontSetLineHeight(Font* font, float lineHeight);
int lovrFontGetKerning(Font* font, unsigned int a, unsigned int b);
Glyph* lovrFontGetGlyph(Font* font, uint32_t codepoint);
void lovrFontAddGlyph(Font* font, Glyph* glyph);
void lovrFontExpandTexture(Font* font);
