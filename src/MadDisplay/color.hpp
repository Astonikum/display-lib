#pragma once
#include <stdint.h>

namespace mad {
// RGB888 source colors; each target chooses its own packed storage format.
enum class Color : uint32_t {
  Off = 0x000000, Black = Off, On = 0xffffff, White = On,
  Red = 0xff0000, Green = 0x00ff00, Blue = 0x0000ff,
  Yellow = 0xffff00, Cyan = 0x00ffff, Magenta = 0xff00ff
};
constexpr Color rgb(uint8_t r,uint8_t g,uint8_t b) {
  return static_cast<Color>((uint32_t(r)<<16) | (uint32_t(g)<<8) | b);
}
constexpr uint8_t red(Color c) { return static_cast<uint8_t>(uint32_t(c)>>16); }
constexpr uint8_t green(Color c) { return static_cast<uint8_t>(uint32_t(c)>>8); }
constexpr uint8_t blue(Color c) { return static_cast<uint8_t>(uint32_t(c)); }
constexpr Color inverted(Color c) { return static_cast<Color>(uint32_t(c)^0xffffffu); }
constexpr uint16_t rgb565(Color c) { return uint16_t((uint16_t(red(c)&0xf8)<<8) | (uint16_t(green(c)&0xfc)<<3) | (blue(c)>>3)); }
constexpr Color fromRGB565(uint16_t value) {
  return rgb(static_cast<uint8_t>((value>>11)*255/31),static_cast<uint8_t>(((value>>5)&63)*255/63),static_cast<uint8_t>((value&31)*255/31));
}
constexpr bool monochromeOn(Color c) { return c == Color::White || (c != Color::Black && uint32_t(red(c))*77+uint32_t(green(c))*150+uint32_t(blue(c))*29 >= 32768); }
enum class PixelFormat : uint8_t { Mono1, Indexed1, Indexed2, RGB565, RGB888, Indexed4 };
enum class ColorMode : uint8_t { Monochrome, Palette, Zoned, RGB };

struct Palette {
  Palette(Color a = Color::Black,Color b = Color::White,Color c = Color::Red,Color d = Color::Yellow,uint8_t size = 2)
      : colors{a,b,c,d}, count(size >= 2 && size <= 16 ? size : 2) {}
  Color colors[16];
  uint8_t count;
  uint8_t index(Color color) const {
    uint8_t best = 0; uint32_t distance = 0xffffffffu;
    for (uint8_t i = 0; i < count && i < 16; ++i) {
      if (color == colors[i]) return i;
      const int32_t r = int32_t(red(color))-red(colors[i]);
      const int32_t g = int32_t(green(color))-green(colors[i]);
      const int32_t b = int32_t(blue(color))-blue(colors[i]);
      const uint32_t score = uint32_t(r*r)*2+uint32_t(g*g)*4+uint32_t(b*b);
      if (score < distance) { distance = score; best = i; }
    }
    return best;
  }
  Color map(Color color) const { return colors[index(color)]; }
};
}  // namespace mad
