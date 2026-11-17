#pragma once
#include "core.hpp"
namespace mad {
struct TextGlyph {
  uint16_t width,height;
  const uint8_t* mask;  // Packed row-major bits, LSB first.
  const uint8_t* rgb565; // Optional color glyph, little-endian bytes.
};
struct GlyphPlacement { uint16_t glyph; int16_t x,y; };
using TextByteReader = uint8_t (*)(const uint8_t*,uint32_t);
struct TextLayout {
  Size size;
  const TextGlyph* glyphs;
  uint16_t glyphCount;
  const GlyphPlacement* placements;
  uint16_t placementCount;
  TextByteReader readByte;
  TextLayout(Size dimensions = {0,0},const TextGlyph* atlas = nullptr,uint16_t glyphsCount = 0,
             const GlyphPlacement* positions = nullptr,uint16_t positionsCount = 0,TextByteReader reader = nullptr)
      : size(dimensions),glyphs(atlas),glyphCount(glyphsCount),placements(positions),placementCount(positionsCount),readByte(reader) {}
};
inline uint8_t textByte(const TextLayout& layout,const uint8_t* data,uint32_t offset) {
  return layout.readByte ? layout.readByte(data,offset) : data[offset];
}
inline void drawTextLayout(Canvas& canvas,const TextLayout& layout,int16_t x,int16_t y,uint8_t scale,Color ink) {
  if (!scale || !layout.glyphs || !layout.placements) return;
  const Rect clip = canvas.viewport();
  for (uint16_t i = 0; i < layout.placementCount; ++i) {
    const GlyphPlacement p = layout.placements[i]; if (p.glyph >= layout.glyphCount) continue;
    const TextGlyph& glyph = layout.glyphs[p.glyph]; if (!glyph.mask) continue;
    const int32_t left = int32_t(x)+int32_t(p.x)*scale,top = int32_t(y)+int32_t(p.y)*scale;
    for (uint16_t row = 0; row < glyph.height; ++row) {
      const int32_t py = top+int32_t(row)*scale;
      if (py+scale <= clip.y || py >= int32_t(clip.y)+clip.height) continue;
      for (uint16_t col = 0; col < glyph.width; ++col) {
        const int32_t px = left+int32_t(col)*scale;
        if (px+scale <= clip.x || px >= int32_t(clip.x)+clip.width || px < -32768 || px > 32767 || py < -32768 || py > 32767) continue;
        const uint32_t index = uint32_t(row)*glyph.width+col;
        if (!(textByte(layout,glyph.mask,index/8)&(1u<<(index%8)))) continue;
        Color color = ink;
        if (glyph.rgb565) color = fromRGB565(uint16_t(textByte(layout,glyph.rgb565,index*2)) | (uint16_t(textByte(layout,glyph.rgb565,index*2+1))<<8));
        canvas.fillRect({int16_t(px),int16_t(py),scale,scale},color);
      }
    }
  }
}
}
