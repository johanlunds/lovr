#include "graphics/font.h"
#include "graphics/graphics.h"
#include "graphics/texture.h"
#include "math/mat4.h"
#include "loaders/font.h"
#include "loaders/texture.h"
#include "util.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

Font* lovrFontCreate(FontData* fontData) {
  Font* font = lovrAlloc(sizeof(Font), lovrFontDestroy);
  if (!font) return NULL;

  font->fontData = fontData;
  font->texture = NULL;
  font->lineHeight = 1.f;
  vec_init(&font->vertices);
  map_init(&font->kerning);

  // Atlas
  int padding = 1;
  font->atlas.x = padding;
  font->atlas.y = padding;
  font->atlas.width = 128;
  font->atlas.height = 128;
  font->atlas.padding = padding;
  map_init(&font->atlas.glyphs);

  // Set initial atlas size
  while (font->atlas.height < 4 * fontData->size) {
    lovrFontExpandTexture(font);
  }

  // Texture
  TextureData* textureData = lovrTextureDataGetBlank(font->atlas.width, font->atlas.height, 0x0, FORMAT_RG);
  font->texture = lovrTextureCreate(textureData);
  lovrTextureSetWrap(font->texture, WRAP_CLAMP, WRAP_CLAMP);
  int swizzle[4] = { GL_RED, GL_RED, GL_RED, GL_GREEN };
  glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzle);

  return font;
}

void lovrFontDestroy(const Ref* ref) {
  Font* font = containerof(ref, Font);
  lovrFontDataDestroy(font->fontData);
  lovrRelease(&font->texture->ref);
  map_deinit(&font->atlas.glyphs);
  map_deinit(&font->kerning);
  vec_deinit(&font->vertices);
  free(font);
}

void lovrFontPrint(Font* font, const char* str, mat4 transform) {
  FontAtlas* atlas = &font->atlas;

  float x = 0;
  float y = -lovrFontGetHeight(font);
  float u = atlas->width;
  float v = atlas->height;

  int len = strlen(str);
  const char* start = str;
  const char* end = str + len;
  unsigned int previous = '\0';
  unsigned int codepoint;
  size_t bytes;

  int lineX = 0;

  vec_reserve(&font->vertices, len * 30);
  vec_clear(&font->vertices);

  while ((bytes = utf8_decode(str, end, &codepoint)) > 0) {

    // Newlines
    if (codepoint == '\n') {

      // Center the line
      while (lineX < font->vertices.length) {
        font->vertices.data[lineX] -= x / 2;
        lineX += 5;
      }

      x = 0;
      y -= font->fontData->size * font->lineHeight;
      previous = '\0';
      str += bytes;
      continue;
    }

    // Kerning
    x += lovrFontGetKerning(font, previous, codepoint);
    previous = codepoint;

    // Get glyph
    Glyph* glyph = lovrFontGetGlyph(font, codepoint);

    // Start over if texture was repacked
    if (u != atlas->width || v != atlas->height) {
      lovrFontPrint(font, start, transform);
      return;
    }

    // Glyph geometry
    int gx = glyph->x;
    int gy = glyph->y;
    int gw = glyph->w;
    int gh = glyph->h;
    int dx = glyph->dx;
    int dy = glyph->dy;

    // Triangles
    if (gw > 0 && gh > 0) {
      float x1 = x + dx;
      float y1 = y + dy;
      float x2 = x1 + gw;
      float y2 = y1 - gh;
      float s1 = gx / u;
      float t1 = gy / v;
      float s2 = (gx + gw) / u;
      float t2 = (gy + gh) / v;

      float v[30] = {
        x1, y1, 0, s1, t1,
        x1, y2, 0, s1, t2,
        x2, y1, 0, s2, t1,
        x2, y1, 0, s2, t1,
        x1, y2, 0, s1, t2,
        x2, y2, 0, s2, t2
      };

      vec_pusharr(&font->vertices, v, 30);
    }

    // Advance cursor
    x += glyph->advance;
    str += bytes;
  }

  // Center the last line
  while (lineX < font->vertices.length) {
    font->vertices.data[lineX] -= x / 2;
    lineX += 5;
  }

  // We override the depth test to LEQUAL to prevent blending issues with glyphs, not great
  CompareMode oldCompareMode = lovrGraphicsGetDepthTest();

  lovrGraphicsPush();
  lovrGraphicsMatrixTransform(transform);
  lovrGraphicsTranslate(0, -y / 2, 0);
  lovrGraphicsSetDepthTest(COMPARE_LEQUAL);
  lovrGraphicsSetShapeData(font->vertices.data, font->vertices.length);
  lovrGraphicsDrawPrimitive(GL_TRIANGLES, font->texture, 0, 1, 0);
  lovrGraphicsSetDepthTest(oldCompareMode);
  lovrGraphicsPop();
}

int lovrFontGetWidth(Font* font, const char* str) {
  float width = 0;
  float x = 0;
  const char* end = str + strlen(str);
  size_t bytes;
  unsigned int previous = '\0';
  unsigned int codepoint;

  while ((bytes = utf8_decode(str, end, &codepoint)) > 0) {

    // Newlines
    if (codepoint == '\n') {
      width = MAX(width, x);
      x = 0;
      previous = '\0';
      str += bytes;
      continue;
    }

    Glyph* glyph = lovrFontGetGlyph(font, codepoint);
    x += glyph->advance + lovrFontGetKerning(font, previous, codepoint);
    previous = codepoint;
    str += bytes;
  }

  return MAX(x, width);
}

int lovrFontGetHeight(Font* font) {
  return font->fontData->height;
}

int lovrFontGetAscent(Font* font) {
  return font->fontData->ascent;
}

int lovrFontGetDescent(Font* font) {
  return font->fontData->descent;
}

int lovrFontGetBaseline(Font* font) {
  return font->fontData->height / 1.25f;
}

float lovrFontGetLineHeight(Font* font) {
  return font->lineHeight;
}

void lovrFontSetLineHeight(Font* font, float lineHeight) {
  font->lineHeight = lineHeight;
}

int lovrFontGetKerning(Font* font, unsigned int left, unsigned int right) {
  char key[12];
  snprintf(key, 12, "%d,%d", left, right);

  int* entry = map_get(&font->kerning, key);
  if (entry) {
    return *entry;
  }

  int kerning = lovrFontDataGetKerning(font->fontData, left, right);
  map_set(&font->kerning, key, kerning);
  return kerning;
}

Glyph* lovrFontGetGlyph(Font* font, uint32_t codepoint) {
  char key[6];
  snprintf(key, 6, "%d", codepoint);

  FontAtlas* atlas = &font->atlas;
  vec_glyph_t* glyphs = &atlas->glyphs;
  Glyph* glyph = map_get(glyphs, key);

  // Add the glyph to the atlas if it isn't there
  if (!glyph) {
    Glyph g;
    lovrFontDataLoadGlyph(font->fontData, codepoint, &g);
    map_set(glyphs, key, g);
    glyph = map_get(glyphs, key);
    lovrFontAddGlyph(font, glyph);
  }

  return glyph;
}

void lovrFontAddGlyph(Font* font, Glyph* glyph) {
  FontAtlas* atlas = &font->atlas;

  // Don't waste space on empty glyphs
  if (glyph->w == 0 && glyph->h == 0) {
    return;
  }

  // If the glyph does not fit, you must acquit (new row)
  if (atlas->x + glyph->w > atlas->width - 2 * atlas->padding) {
    atlas->x = atlas->padding;
    atlas->y += atlas->rowHeight + atlas->padding;
    atlas->rowHeight = 0;
  }

  // Expand the texture if needed. Expanding the texture re-adds all the glyphs, so we can return.
  if (atlas->y + glyph->h > atlas->height - 2 * atlas->padding) {
    lovrFontExpandTexture(font);
    return;
  }

  // Keep track of glyph's position in the atlas
  glyph->x = atlas->x;
  glyph->y = atlas->y;

  // Paste glyph into texture
  lovrGraphicsBindTexture(font->texture);
  glTexSubImage2D(GL_TEXTURE_2D, 0, atlas->x, atlas->y, glyph->w, glyph->h, GL_RG, GL_UNSIGNED_BYTE, glyph->data);

  // Advance atlas cursor
  atlas->x += glyph->w + atlas->padding;
  atlas->rowHeight = MAX(atlas->rowHeight, glyph->h);
}

void lovrFontExpandTexture(Font* font) {
  FontAtlas* atlas = &font->atlas;

  if (atlas->width == atlas->height) {
    atlas->width *= 2;
  } else {
    atlas->height *= 2;
  }

  if (!font->texture) {
    return;
  }

  // Resize the texture storage
  while (glGetError());
  lovrTextureDataResize(font->texture->textureData, atlas->width, atlas->height, 0x0);
  lovrTextureRefresh(font->texture);
  if (glGetError()) {
    error("Problem expanding font texture (out of space?)");
  }

  // Reset the cursor
  atlas->x = atlas->padding;
  atlas->y = atlas->padding;
  atlas->rowHeight = 0;

  // Re-pack all the glyphs
  const char* key;
  map_iter_t iter = map_iter(&atlas->glyphs);
  while ((key = map_next(&atlas->glyphs, &iter)) != NULL) {
    Glyph* glyph = map_get(&atlas->glyphs, key);
    lovrFontAddGlyph(font, glyph);
  }
}
