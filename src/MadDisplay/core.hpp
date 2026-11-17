#pragma once

#include <stdint.h>
#include <stddef.h>
#include "animation.hpp"
#include "callback.hpp"
#include "color.hpp"
#include "unicode.hpp"

namespace mad {

struct Size { int16_t width, height; };
struct Point { int16_t x, y; };
struct Rect { int16_t x, y, width, height; };
inline bool containsPoint(Rect rect, Point point) {
  return point.x >= rect.x && point.y >= rect.y && point.x < int32_t(rect.x)+rect.width && point.y < int32_t(rect.y)+rect.height;
}
inline Rect unionBounds(Rect a, Rect b) {
  if (a.width <= 0 || a.height <= 0) return b;
  if (b.width <= 0 || b.height <= 0) return a;
  const int32_t left = a.x < b.x ? a.x : b.x, top = a.y < b.y ? a.y : b.y;
  const int32_t right = int32_t(a.x)+a.width > int32_t(b.x)+b.width ? int32_t(a.x)+a.width : int32_t(b.x)+b.width;
  const int32_t bottom = int32_t(a.y)+a.height > int32_t(b.y)+b.height ? int32_t(a.y)+a.height : int32_t(b.y)+b.height;
  return {static_cast<int16_t>(left),static_cast<int16_t>(top),static_cast<int16_t>(right-left > 32767 ? 32767 : right-left),static_cast<int16_t>(bottom-top > 32767 ? 32767 : bottom-top)};
}
inline Rect intersectBounds(Rect a, Rect b) {
  const int32_t left = a.x > b.x ? a.x : b.x, top = a.y > b.y ? a.y : b.y;
  const int32_t right = int32_t(a.x)+a.width < int32_t(b.x)+b.width ? int32_t(a.x)+a.width : int32_t(b.x)+b.width;
  const int32_t bottom = int32_t(a.y)+a.height < int32_t(b.y)+b.height ? int32_t(a.y)+a.height : int32_t(b.y)+b.height;
  return {int16_t(left),int16_t(top),int16_t(right > left ? right-left : 0),int16_t(bottom > top ? bottom-top : 0)};
}
struct Insets {
  Insets(int16_t left = 0, int16_t top = 0, int16_t right = 0, int16_t bottom = 0)
      : left(left), top(top), right(right), bottom(bottom) {}
  int16_t left, top, right, bottom;
};

struct ColorRegion { Rect bounds; Color color; };
// Regions describe physical emission, not independently addressable colors.
// Region storage must outlive the profile; any number of regions is supported.
struct ColorProfile {
  ColorMode mode = ColorMode::RGB;
  Palette palette;
  Color ink = Color::White;
  const ColorRegion* regions = nullptr;
  uint8_t regionCount = 0;
  Color map(Color color,int16_t x,int16_t y) const {
    if (mode == ColorMode::RGB) return color;
    if (mode == ColorMode::Palette) return palette.map(color);
    if (!monochromeOn(color)) return Color::Black;
    if (mode == ColorMode::Zoned && regions) {
      for (uint8_t i = 0; i < regionCount; ++i) if (containsPoint(regions[i].bounds,{x,y})) return regions[i].color;
    }
    return ink;
  }
};
enum class Unit : uint8_t { Pixels, Percent, Max, Auto };
enum class SizeMode : uint8_t { Fit, Max };
enum class HorizontalAlign : uint8_t { Left, Center, Right };
enum class VerticalAlign : uint8_t { Top, Center, Bottom };

// The default Canvas text contract matches the classic 5x7 OLED glyph with
// one blank row of line spacing (the SSD1306/Adafruit default cell is 6x8).
static constexpr uint8_t kTextAdvance = 6;
static constexpr uint8_t kTextHeight = 8;
static constexpr uint8_t kGlyphWidth = 5;
static constexpr uint8_t kGlyphHeight = 7;
static constexpr uint8_t kDefaultGap = 1;

struct Length {
  Length(int16_t value = 0, Unit unit = Unit::Pixels) : value(value), unit(unit) {}
  static Length autoSize() { return {0, Unit::Auto}; }
  static Length px(int16_t value) { return {value, Unit::Pixels}; }
  static Length percent(int16_t value) { return {value, Unit::Percent}; }
  static Length fill() { return {0, Unit::Max}; }
  static Length (max)() { return fill(); }
  int16_t value;
  Unit unit;
};

// Percentages are relative to the current container. For direct Page children,
// that container is the full display.
struct Frame {
  Frame(Length x = Length::px(0), Length y = Length::px(0),
        Length width = Length::autoSize(), Length height = Length::autoSize())
      : x(x), y(y), width(width), height(height) {}
  static Frame px(int16_t x, int16_t y, int16_t width, int16_t height) {
    return {Length::px(x), Length::px(y), Length::px(width), Length::px(height)};
  }
  static Frame percent(int16_t x, int16_t y, int16_t width, int16_t height) {
    return {Length::percent(x), Length::percent(y), Length::percent(width), Length::percent(height)};
  }
  static Frame fill(int16_t x = 0,int16_t y = 0) {
    return {Length::px(x),Length::px(y),Length::fill(),Length::fill()};
  }
  static Frame (max)(int16_t x = 0,int16_t y = 0) { return fill(x,y); }
  Length x, y, width, height;
};

// Size-only configuration for flow components. It deliberately has no
// position; use Frame only when placing a child in FreeLayout.
struct SizeSpec {
  SizeSpec(Length width = Length::autoSize(), Length height = Length::autoSize())
      : width(width), height(height) {}
  static SizeSpec px(int16_t width, int16_t height) { return {Length::px(width), Length::px(height)}; }
  static SizeSpec percent(int16_t width, int16_t height) { return {Length::percent(width), Length::percent(height)}; }
  static SizeSpec fill() { return {Length::fill(),Length::fill()}; }
  static SizeSpec (max)() { return fill(); }
  Length width, height;
};

// Glyph columns contain ceil(height/8) bytes each, top-to-bottom pages, LSB first.
// UTF-32 scalar keys and 32-bit counts cover every Unicode plane. Shaped text
// (joining, bidi and clusters) uses TextLayout instead of character lookup.
struct BitmapGlyph {
  uint32_t codepoint;
  uint16_t width;
  const uint8_t* columns;
};

struct BitmapFont {
  BitmapFont(uint16_t height = 0, uint16_t lineHeight = 0, uint8_t spacing = 1,
             const BitmapGlyph* glyphs = nullptr, uint32_t glyphCount = 0,
             uint32_t fallback = '?', bool sorted = false)
      : height(height), lineHeight(lineHeight), spacing(spacing), glyphs(glyphs),
        glyphCount(glyphCount), fallback(fallback), sorted(sorted) {}
  uint16_t height;
  uint16_t lineHeight;
  uint8_t spacing;
  const BitmapGlyph* glyphs;
  uint32_t glyphCount;
  uint32_t fallback;
  bool sorted;
};

inline const uint8_t* defaultGlyphFor(char character) {
  static const uint8_t font[38][5] = {
      {0x7E,0x09,0x09,0x09,0x7E},{0x7F,0x49,0x49,0x49,0x36},{0x3E,0x41,0x41,0x41,0x22},
      {0x7F,0x41,0x41,0x22,0x1C},{0x7F,0x49,0x49,0x49,0x41},{0x7F,0x09,0x09,0x09,0x01},
      {0x3E,0x41,0x49,0x49,0x7A},{0x7F,0x08,0x08,0x08,0x7F},{0x00,0x41,0x7F,0x41,0x00},
      {0x20,0x40,0x41,0x3F,0x01},{0x7F,0x08,0x14,0x22,0x41},{0x7F,0x40,0x40,0x40,0x40},
      {0x7F,0x02,0x0C,0x02,0x7F},{0x7F,0x04,0x08,0x10,0x7F},{0x3E,0x41,0x41,0x41,0x3E},
      {0x7F,0x09,0x09,0x09,0x06},{0x3E,0x41,0x51,0x21,0x5E},{0x7F,0x09,0x19,0x29,0x46},
      {0x46,0x49,0x49,0x49,0x31},{0x01,0x01,0x7F,0x01,0x01},{0x3F,0x40,0x40,0x40,0x3F},
      {0x1F,0x20,0x40,0x20,0x1F},{0x7F,0x20,0x18,0x20,0x7F},{0x63,0x14,0x08,0x14,0x63},
      {0x07,0x08,0x70,0x08,0x07},{0x61,0x51,0x49,0x45,0x43},
      {0x3E,0x45,0x49,0x51,0x3E},{0x00,0x42,0x7F,0x40,0x00},{0x42,0x61,0x51,0x49,0x46},
      {0x21,0x41,0x45,0x4B,0x31},{0x18,0x14,0x12,0x7F,0x10},{0x27,0x45,0x45,0x45,0x39},
      {0x3C,0x4A,0x49,0x49,0x30},{0x01,0x71,0x09,0x05,0x03},{0x36,0x49,0x49,0x49,0x36},
      {0x06,0x49,0x49,0x29,0x1E},{0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0x5F,0x00,0x00}
  };
  static const uint8_t lowercase[26][5] = {
      {0x20,0x54,0x54,0x54,0x78},{0x7f,0x48,0x48,0x48,0x30},{0x38,0x44,0x44,0x44,0x20},
      {0x30,0x48,0x48,0x48,0x7f},{0x38,0x54,0x54,0x54,0x18},{0x08,0x7e,0x09,0x01,0x02},
      {0x0c,0x52,0x52,0x52,0x3e},{0x7f,0x08,0x04,0x04,0x78},{0x00,0x44,0x7d,0x40,0x00},
      {0x20,0x40,0x44,0x3d,0x00},{0x7f,0x10,0x28,0x44,0x00},{0x00,0x41,0x7f,0x40,0x00},
      {0x7c,0x04,0x18,0x04,0x78},{0x7c,0x08,0x04,0x04,0x78},{0x38,0x44,0x44,0x44,0x38},
      {0x7c,0x14,0x14,0x14,0x08},{0x08,0x14,0x14,0x18,0x7c},{0x7c,0x08,0x04,0x04,0x08},
      {0x48,0x54,0x54,0x54,0x24},{0x04,0x3f,0x44,0x40,0x20},{0x3c,0x40,0x40,0x20,0x7c},
      {0x1c,0x20,0x40,0x20,0x1c},{0x3c,0x40,0x30,0x40,0x3c},{0x44,0x28,0x10,0x28,0x44},
      {0x0c,0x50,0x50,0x50,0x3c},{0x44,0x64,0x54,0x4c,0x44}
  };
  if (character >= 'a' && character <= 'z') return lowercase[character-'a'];
  if (character >= 'A' && character <= 'Z') return font[character - 'A'];
  if (character >= '0' && character <= '9') return font[26 + character - '0'];
  return character == '!' ? font[37] : font[36];
}


class Canvas {
 public:
  virtual ~Canvas() = default;
  virtual Size size() const = 0;
  virtual PixelFormat format() const { return PixelFormat::RGB888; }
  virtual Rect viewport() const { const Size s = size(); return {0,0,s.width,s.height}; }
  virtual void clear(Color color = Color::Off) = 0;
  virtual void pixel(int16_t x, int16_t y, Color color) = 0;
  virtual void fixedPixel(int16_t x,int16_t y,Color color) { pixel(x,y,color); }
  virtual void fillSpan(int16_t x,int16_t y,int16_t length,Color color) {
    for (int32_t i = 0; i < length; ++i) pixel(static_cast<int16_t>(x+i),y,color);
  }
  virtual void fillRect(Rect rect, Color color) {
    const Rect visible = intersectBounds(rect,viewport());
    for (int32_t y = visible.y; y < int32_t(visible.y)+visible.height; ++y)
      if (visible.width > 0) fillSpan(visible.x,int16_t(y),visible.width,color);
  }
  virtual void strokeRect(Rect rect, Color color) {
    if (rect.width <= 0 || rect.height <= 0) return;
    fillRect({rect.x, rect.y, rect.width, 1}, color);
    fillRect({rect.x, static_cast<int16_t>(rect.y + rect.height - 1), rect.width, 1}, color);
    fillRect({rect.x, rect.y, 1, rect.height}, color);
    fillRect({static_cast<int16_t>(rect.x + rect.width - 1), rect.y, 1, rect.height}, color);
  }
  virtual void text(int16_t x, int16_t y, const char* value, uint8_t scale, Color color) = 0;
  void text(int16_t x, int16_t y, const char* value, const BitmapFont& font,
            uint8_t scale, Color color);
  virtual void present() = 0;
};

// Rasterize through pixel() so nested clips and transforms also apply to text.
inline Size defaultTextSize(const char* value,uint8_t scale = 1) {
  int32_t width = 0,widest = 0,lines = 1;
  const char* cursor = value ? value : "";
  while (*cursor) {
    const uint32_t cp = nextUtf8Codepoint(cursor);
    if (cp == '\r') continue;
    if (cp == '\n') { if (width > widest) widest = width; width = 0; if (lines < 4096) ++lines; }
    else { width += (cp == '\t' ? 4 : 1)*kTextAdvance; if (width > 32767) width = 32767; }
  }
  if (width > widest) widest = width;
  const int32_t height = ((lines-1)*kTextHeight+kGlyphHeight)*scale;
  const int32_t size = (widest ? widest-(kTextAdvance-kGlyphWidth) : 0)*scale;
  return {int16_t(size > 32767 ? 32767 : size),int16_t(height > 32767 ? 32767 : height)};
}
inline const uint8_t* findDefaultGlyph(uint32_t cp) {
  static const uint8_t cyrillic[32][5] = {
    {0x7e,0x09,0x09,0x09,0x7e}, // А
    {0x7f,0x49,0x49,0x49,0x31}, // Б
    {0x7f,0x49,0x49,0x49,0x36}, // В
    {0x7f,0x01,0x01,0x01,0x01}, // Г
    {0x60,0x3e,0x21,0x3f,0x60}, // Д
    {0x7f,0x49,0x49,0x49,0x41}, // Е
    {0x77,0x08,0x7f,0x08,0x77}, // Ж
    {0x22,0x41,0x49,0x49,0x36}, // З
    {0x7f,0x10,0x08,0x04,0x7f}, // И
    {0x7c,0x21,0x12,0x09,0x7c}, // Й
    {0x7f,0x08,0x14,0x22,0x41}, // К
    {0x40,0x3e,0x01,0x01,0x7f}, // Л
    {0x7f,0x02,0x0c,0x02,0x7f}, // М
    {0x7f,0x08,0x08,0x08,0x7f}, // Н
    {0x3e,0x41,0x41,0x41,0x3e}, // О
    {0x7f,0x01,0x01,0x01,0x7f}, // П
    {0x7f,0x09,0x09,0x09,0x06}, // Р
    {0x3e,0x41,0x41,0x41,0x22}, // С
    {0x01,0x01,0x7f,0x01,0x01}, // Т
    {0x07,0x48,0x48,0x48,0x3f}, // У
    {0x1c,0x22,0x7f,0x22,0x1c}, // Ф
    {0x63,0x14,0x08,0x14,0x63}, // Х
    {0x3f,0x20,0x20,0x3f,0x60}, // Ц
    {0x07,0x08,0x08,0x08,0x7f}, // Ч
    {0x7f,0x40,0x7f,0x40,0x7f}, // Ш
    {0x3f,0x20,0x3f,0x20,0x7f}, // Щ
    {0x01,0x7f,0x48,0x48,0x30}, // Ъ
    {0x7f,0x48,0x30,0x00,0x7f}, // Ы
    {0x7f,0x48,0x48,0x48,0x30}, // Ь
    {0x22,0x41,0x49,0x49,0x3e}, // Э
    {0x7f,0x08,0x3e,0x41,0x3e}, // Ю
    {0x46,0x29,0x19,0x09,0x7f}  // Я
  };
  if (cp >= 0x410 && cp <= 0x42f) return cyrillic[cp-0x410];
  static const uint8_t cyrillicLowercase[32][5] = {
    {0x20,0x54,0x54,0x54,0x78}, // а
    {0x3c,0x4a,0x4a,0x49,0x30}, // б
    {0x7c,0x54,0x54,0x54,0x28}, // в
    {0x7c,0x04,0x04,0x04,0x04}, // г
    {0x60,0x3c,0x24,0x3c,0x60}, // д
    {0x38,0x54,0x54,0x54,0x18}, // е
    {0x6c,0x10,0x7c,0x10,0x6c}, // ж
    {0x28,0x44,0x54,0x54,0x28}, // з
    {0x7c,0x20,0x10,0x08,0x7c}, // и
    {0x78,0x42,0x24,0x12,0x78}, // й
    {0x7c,0x10,0x28,0x44,0x00}, // к
    {0x40,0x3c,0x04,0x04,0x7c}, // л
    {0x7c,0x08,0x10,0x08,0x7c}, // м
    {0x7c,0x10,0x10,0x10,0x7c}, // н
    {0x38,0x44,0x44,0x44,0x38}, // о
    {0x7c,0x04,0x04,0x04,0x7c}, // п
    {0x7c,0x14,0x14,0x14,0x08}, // р
    {0x38,0x44,0x44,0x44,0x20}, // с
    {0x7c,0x04,0x7c,0x04,0x7c}, // т
    {0x0c,0x50,0x50,0x50,0x3c}, // у
    {0x18,0x24,0x7f,0x24,0x18}, // ф
    {0x44,0x28,0x10,0x28,0x44}, // х
    {0x3c,0x20,0x20,0x3c,0x60}, // ц
    {0x0c,0x10,0x10,0x10,0x7c}, // ч
    {0x7c,0x40,0x7c,0x40,0x7c}, // ш
    {0x3c,0x20,0x3c,0x20,0x7c}, // щ
    {0x04,0x7c,0x50,0x50,0x20}, // ъ
    {0x7c,0x50,0x20,0x00,0x7c}, // ы
    {0x7c,0x50,0x50,0x50,0x20}, // ь
    {0x28,0x44,0x54,0x54,0x38}, // э
    {0x7c,0x10,0x38,0x44,0x38}, // ю
    {0x48,0x34,0x14,0x14,0x7c}  // я
  };
  if (cp >= 0x430 && cp <= 0x44f) return cyrillicLowercase[cp-0x430];
  static const uint8_t yo[5] = {0x7c,0x55,0x54,0x55,0x44};
  static const uint8_t yi[5] = {0x00,0x45,0x7c,0x45,0x00};
  static const uint8_t ye[5] = {0x3e,0x49,0x49,0x49,0x22};
  static const uint8_t ghe[5] = {0x7e,0x02,0x02,0x02,0x03};
  static const uint8_t shortU[5] = {0x0c,0x51,0x52,0x51,0x3c};
  static const uint8_t yoLower[5] = {0x38,0x55,0x54,0x55,0x18};
  static const uint8_t yiLower[5] = {0,0x45,0x7c,0x41,0};
  static const uint8_t yeLower[5] = {0x38,0x54,0x54,0x54,0x28};
  static const uint8_t gheLower[5] = {0x7c,0x04,0x04,0x04,0x06};
  static const uint8_t shortULower[5] = {0x0c,0x51,0x52,0x51,0x3c};
  if (cp == 0x401) return yo;
  if (cp == 0x451) return yoLower;
  if (cp == 0x407) return yi;
  if (cp == 0x457) return yiLower;
  if (cp == 0x404) return ye;
  if (cp == 0x454) return yeLower;
  if (cp == 0x490) return ghe;
  if (cp == 0x491) return gheLower;
  if (cp == 0x40e) return shortU;
  if (cp == 0x45e) return shortULower;
  if (cp == 0x406) return defaultGlyphFor('I');
  if (cp == 0x456) return defaultGlyphFor('i');
  static const uint8_t dash[5] = {8,8,8,8,8},dot[5] = {0,0x60,0x60,0,0},comma[5] = {0,0x40,0x30,0,0};
  static const uint8_t colon[5] = {0,0x36,0x36,0,0},quote[5] = {0,3,0,3,0},apostrophe[5] = {0,3,0,0,0};
  static const uint8_t underscore[5] = {0x40,0x40,0x40,0x40,0x40},plus[5] = {8,8,0x3e,8,8};
  static const uint8_t question[5] = {2,1,0x51,9,6},slash[5] = {0x40,0x30,8,6,1};
  static const uint8_t punctuation[][5] = {
    {0x14,0x7f,0x14,0x7f,0x14}, // #
    {0x24,0x2a,0x7f,0x2a,0x12}, // $
    {0x23,0x13,0x08,0x64,0x62}, // %
    {0x36,0x49,0x55,0x22,0x50}, // &
    {0,0x1c,0x22,0x41,0},       // (
    {0,0x41,0x22,0x1c,0},       // )
    {0x14,0x08,0x3e,0x08,0x14}, // *
    {0,0x40,0x36,0,0},          // ;
    {0x08,0x14,0x22,0x41,0},    // <
    {0x14,0x14,0x14,0x14,0x14}, // =
    {0,0x41,0x22,0x14,0x08},    // >
    {0x3e,0x41,0x5d,0x55,0x1e}, // @
    {0,0x7f,0x41,0x41,0},       // [
    {1,6,8,0x30,0x40},         // backslash
    {0,0x41,0x41,0x7f,0},       // ]
    {4,2,1,2,4},               // ^
    {0,1,2,0,0},               // `
    {0,8,0x36,0x41,0},         // {
    {0,0,0x7f,0,0},            // |
    {0,0x41,0x36,8,0},         // }
    {8,4,8,0x10,8}             // ~
  };
  const char* symbols = "#$%&()*;<=>@[\\]^`{|}~";
  for (unsigned i = 0; symbols[i]; ++i) if (cp == uint8_t(symbols[i])) return punctuation[i];
  if (cp == 0xa0) return defaultGlyphFor(' ');
  if (cp == '-') return dash;
  if (cp == '.') return dot;
  if (cp == ',') return comma;
  if (cp == ':') return colon;
  if (cp == '\'') return apostrophe;
  if (cp == '"') return quote;
  if (cp == '_') return underscore;
  if (cp == '+') return plus;
  if (cp == '?') return question;
  if (cp == '/') return slash;
  if (cp == ' ' || cp == '!' || (cp >= 'A' && cp <= 'Z') || (cp >= 'a' && cp <= 'z') || (cp >= '0' && cp <= '9')) return defaultGlyphFor(static_cast<char>(cp));
  return nullptr;
}
inline const uint8_t* defaultUnicodeGlyph(uint32_t cp) {
  static const uint8_t missing[5] = {0x7f,0x41,0x55,0x41,0x7f};
  const uint8_t* glyph = findDefaultGlyph(cp);
  return glyph ? glyph : missing;
}
inline void drawDefaultText(Canvas& canvas,int16_t x,int16_t y,const char* value,uint8_t scale,Color color) {
  if (!value || !scale) return;
  int32_t px = x,py = y; const char* cursor = value;
  while (*cursor) {
    const uint32_t cp = nextUtf8Codepoint(cursor);
    if (cp == '\r') continue;
    if (cp == '\n') { px = x; py += kTextHeight*scale; if (py > 32767) return; continue; }
    if (cp == '\t') { px += 4*kTextAdvance*scale; continue; }
    const uint8_t* glyph = defaultUnicodeGlyph(cp);
    if (px <= 32767 && py <= 32767) for (uint8_t column = 0; column < 5; ++column) for (uint8_t row = 0; row < kGlyphHeight; ++row)
      if (glyph[column] & (1u<<row)) canvas.fillRect({int16_t(px+column*scale),int16_t(py+row*scale),scale,scale},color);
    px += kTextAdvance*scale;
  }
}

// A stack-only view: no extra framebuffer, and nested views intersect naturally.
class CanvasView final : public Canvas {
 public:
  using Canvas::text;
  CanvasView(Canvas& target, Rect clip, int16_t dx = 0, int16_t dy = 0)
      : target_(target), clip_(intersectBounds(clip,target.viewport())), dx_(dx), dy_(dy) {}
  Size size() const override { return target_.size(); }
  PixelFormat format() const override { return target_.format(); }
  Rect viewport() const override { return {static_cast<int16_t>(clip_.x - dx_),static_cast<int16_t>(clip_.y - dy_),clip_.width,clip_.height}; }
  void clear(Color color = Color::Off) override {
    fillRect(viewport(), color);
  }
  void pixel(int16_t x,int16_t y,Color color) override { forwardPixel(x,y,color,false); }
  void fixedPixel(int16_t x,int16_t y,Color color) override { forwardPixel(x,y,color,true); }
  void forwardPixel(int16_t x, int16_t y, Color color,bool fixed) {
    const int32_t tx = static_cast<int32_t>(x) + dx_;
    const int32_t ty = static_cast<int32_t>(y) + dy_;
    if (tx >= clip_.x && ty >= clip_.y && tx < static_cast<int32_t>(clip_.x) + clip_.width &&
        ty < static_cast<int32_t>(clip_.y) + clip_.height) {
      if (fixed) target_.fixedPixel(static_cast<int16_t>(tx),static_cast<int16_t>(ty),color);
      else target_.pixel(static_cast<int16_t>(tx),static_cast<int16_t>(ty),color);
    }
  }
  void fillSpan(int16_t x,int16_t y,int16_t length,Color color) override {
    const int32_t ty = int32_t(y)+dy_;
    if (length <= 0 || ty < clip_.y || ty >= int32_t(clip_.y)+clip_.height) return;
    int32_t left = int32_t(x)+dx_, right = left+length;
    if (left < clip_.x) left = clip_.x;
    if (right > int32_t(clip_.x)+clip_.width) right = int32_t(clip_.x)+clip_.width;
    if (left < right) target_.fillSpan(static_cast<int16_t>(left),static_cast<int16_t>(ty),static_cast<int16_t>(right-left),color);
  }
  void fillRect(Rect rect, Color color) override {
    const Rect visible = intersectBounds(rect,viewport());
    if (visible.width <= 0 || visible.height <= 0) return;
    target_.fillRect({int16_t(visible.x+dx_),int16_t(visible.y+dy_),visible.width,visible.height},color);
  }
  void text(int16_t x, int16_t y, const char* value, uint8_t scale, Color color) override {
    drawDefaultText(*this, x, y, value, scale, color);
  }
  void present() override {}
 private:
  Canvas& target_;
  Rect clip_;
  int16_t dx_, dy_;
};

// Apply a physical color profile once, after geometry/focus transforms.
// Monochrome hardware keeps logical bits; RGB targets can preview emitter hues.
class ColorProfileCanvas final : public Canvas {
 public:
  using Canvas::text;
  ColorProfileCanvas(Canvas& target,const ColorProfile& profile) : target_(target), profile_(profile) {}
  Size size() const override { return target_.size(); }
  Rect viewport() const override { return target_.viewport(); }
  PixelFormat format() const override { return target_.format(); }
  void clear(Color color = Color::Off) override {
    if (profile_.mode == ColorMode::RGB) target_.clear(color);
    else fillRect(viewport(),color);
  }
  void pixel(int16_t x,int16_t y,Color color) override { target_.pixel(x,y,mapped(color,x,y)); }
  void fixedPixel(int16_t x,int16_t y,Color color) override { target_.fixedPixel(x,y,mapped(color,x,y)); }
  void fillSpan(int16_t x,int16_t y,int16_t length,Color color) override {
    if (profile_.mode != ColorMode::Zoned || target_.format() == PixelFormat::Mono1) {
      target_.fillSpan(x,y,length,mapped(color,x,y)); return;
    }
    const Rect clip = viewport();
    if (y < clip.y || y >= int32_t(clip.y)+clip.height || length <= 0) return;
    int32_t left = x > clip.x ? x : clip.x;
    const int32_t right = int32_t(x)+length < int32_t(clip.x)+clip.width ? int32_t(x)+length : int32_t(clip.x)+clip.width;
    if (left >= right) return;
    Color runColor = mapped(color,left,y); int32_t run = left;
    for (++left; left <= right; ++left) {
      const Color next = left < right ? mapped(color,left,y) : Color::Off;
      if (left == right || next != runColor) {
        target_.fillSpan(run,y,left-run,runColor); run = left; runColor = next;
      }
    }
  }
  void text(int16_t x,int16_t y,const char* value,uint8_t scale,Color color) override { drawDefaultText(*this,x,y,value,scale,color); }
  void present() override { target_.present(); }
 private:
  Color mapped(Color color,int16_t x,int16_t y) const {
    if (target_.format() == PixelFormat::Mono1 && (profile_.mode == ColorMode::Monochrome || profile_.mode == ColorMode::Zoned))
      return monochromeOn(color) ? Color::On : Color::Off;
    return profile_.map(color,x,y);
  }
  Canvas& target_;
  ColorProfile profile_;
};

inline const BitmapGlyph* exactBitmapGlyph(const BitmapFont& font,uint32_t codepoint) {
  if (!font.glyphs) return nullptr;
  if (font.sorted) {
    uint32_t low = 0,high = font.glyphCount;
    while (low < high) { const uint32_t mid = low+(high-low)/2; if (font.glyphs[mid].codepoint < codepoint) low = mid+1; else high = mid; }
    return low < font.glyphCount && font.glyphs[low].codepoint == codepoint ? &font.glyphs[low] : nullptr;
  }
  for (uint32_t i = 0; i < font.glyphCount; ++i) if (font.glyphs[i].codepoint == codepoint) return &font.glyphs[i];
  return nullptr;
}
inline const BitmapGlyph* findBitmapGlyph(const BitmapFont& font,uint32_t codepoint) {
  const BitmapGlyph* glyph = exactBitmapGlyph(font,codepoint);
  return glyph ? glyph : codepoint == font.fallback ? nullptr : exactBitmapGlyph(font,font.fallback);
}

inline uint16_t bitmapLineHeight(const BitmapFont& font) {
  return font.lineHeight ? font.lineHeight : font.height;
}

inline Size bitmapTextSize(const char* value,const BitmapFont& font,uint8_t scale = 1) {
  if (!value || !scale) return {0,int16_t(bitmapLineHeight(font) > 32767 ? 32767 : bitmapLineHeight(font))};
  int32_t lineWidth = 0,maxWidth = 0; uint32_t lines = 1; const char* cursor = value;
  const auto trim = [&](int32_t width) { return width ? width-int32_t(font.spacing)*scale : 0; };
  while (*cursor) {
    const uint32_t codepoint = nextUtf8Codepoint(cursor);
    if (codepoint == '\r') continue;
    if (codepoint == '\n') { if (trim(lineWidth) > maxWidth) maxWidth = trim(lineWidth); lineWidth = 0; if (lines < 32767) ++lines; continue; }
    const BitmapGlyph* glyph = findBitmapGlyph(font,codepoint); if (!glyph) continue;
    lineWidth += (int32_t(glyph->width)+font.spacing)*scale; if (lineWidth > 32767) lineWidth = 32767;
  }
  if (trim(lineWidth) > maxWidth) maxWidth = trim(lineWidth);
  const uint64_t height = uint64_t(lines)*bitmapLineHeight(font)*scale;
  return {int16_t(maxWidth > 32767 ? 32767 : maxWidth),int16_t(height > 32767 ? 32767 : height)};
}

inline void drawBitmapText(Canvas& canvas, int16_t x, int16_t y, const char* value,
                           const BitmapFont& font, uint8_t scale, Color color) {
  if (!value || !scale) return;
  int32_t cursorX = x,cursorY = y;
  const int32_t lineHeight = int32_t(bitmapLineHeight(font))*scale;
  const char* cursor = value;
  while (*cursor) {
    const uint32_t codepoint = nextUtf8Codepoint(cursor);
    if (codepoint == '\r') continue;
    if (codepoint == '\n') {
      cursorX = x;
      cursorY += lineHeight; if (cursorY > 32767) return;
      continue;
    }
    const BitmapGlyph* glyph = findBitmapGlyph(font, codepoint);
    if (!glyph) continue;
    if (cursorX > 32767) continue;
    for (uint16_t column = 0; column < glyph->width; ++column) {
      for (uint16_t row = 0; row < font.height; ++row) {
        const uint8_t bits = glyph->columns ? glyph->columns[uint32_t(column)*((font.height+7u)/8u)+row/8] : 0;
        if (!(bits & (1u << (row%8)))) continue;
        for (uint8_t dx = 0; dx < scale; ++dx)
          for (uint8_t dy = 0; dy < scale; ++dy)
            canvas.pixel(static_cast<int16_t>(cursorX + column * scale + dx),
                         static_cast<int16_t>(cursorY + row * scale + dy), color);
      }
    }
    cursorX += (int32_t(glyph->width)+font.spacing)*scale;
  }
}

inline void Canvas::text(int16_t x, int16_t y, const char* value,
                         const BitmapFont& font, uint8_t scale, Color color) {
  drawBitmapText(*this, x, y, value, font, scale, color);
}

enum class Action : uint8_t {
  Down,
  Up,
  Left,
  Right,
  Next,
  Previous,
  Use,
  Back
};

class Layout;

class Component {
 public:
  Component() : frame_(), bounds_({0, 0, 0, 0}), hasFrame_(false) {}
  explicit Component(Frame frame) : frame_(frame), bounds_({0, 0, 0, 0}), hasFrame_(true) {}
  explicit Component(SizeSpec size) : frame_(Length::px(0), Length::px(0), size.width, size.height), bounds_({0, 0, 0, 0}), hasFrame_(true) {}
  virtual ~Component() = default;
  virtual bool valid() const { return buildOK_; }

  Frame frame() const { return frame_; }
  void setFrame(Frame frame) {
    frame_ = frame;
    hasFrame_ = true;
    widthModeSet_ = false;
    heightModeSet_ = false;
  }
  void setSize(SizeSpec size) {
    frame_.width = size.width;
    frame_.height = size.height;
    hasFrame_ = true;
    widthModeSet_ = false;
    heightModeSet_ = false;
  }
  void setWidth(Length width) { frame_.width = width; hasFrame_ = true; widthModeSet_ = false; }
  void setHeight(Length height) { frame_.height = height; hasFrame_ = true; heightModeSet_ = false; }
  Rect bounds() const { return bounds_; }
  bool hasFrame() const { return hasFrame_; }
  bool visible() const { return visible_; }
  void setVisible(bool visible) {
    visible_ = visible;
    if (!visible_) setFocused(false);
  }
  bool enabled() const { return enabled_; }
  void setEnabled(bool enabled) {
    enabled_ = enabled;
    if (!enabled_) setFocused(false);
  }

  void setSizeMode(SizeMode width, SizeMode height) {
    widthMode_ = width; heightMode_ = height; widthModeSet_ = true; heightModeSet_ = true;
  }
  void setWidthMode(SizeMode mode) { widthMode_ = mode; widthModeSet_ = true; }
  void setHeightMode(SizeMode mode) { heightMode_ = mode; heightModeSet_ = true; }
  SizeMode widthMode() const { return widthMode_; }
  SizeMode heightMode() const { return heightMode_; }
  bool hasWidthMode() const { return widthModeSet_; }
  bool hasHeightMode() const { return heightModeSet_; }

  // The size requested before a flow container applies its axis policy.
  // Explicit SizeSpec/Frame dimensions win; a no-frame component contributes
  // its intrinsic size.
  Size requestedSize(Rect parent) const {
    const Size intrinsic = !hasFrame_ || frame_.width.unit == Unit::Auto || frame_.height.unit == Unit::Auto ? preferredSize() : Size{0,0};
    return {hasFrame_ && frame_.width.unit != Unit::Auto ? nonnegative(resolveLength(frame_.width,parent.width)) : intrinsic.width,
            hasFrame_ && frame_.height.unit != Unit::Auto ? nonnegative(resolveLength(frame_.height,parent.height)) : intrinsic.height};
  }

  // Percent/fill need a definite parent; they do not feed back into auto sizing.
  Size intrinsicSize() const {
    Size result = !hasFrame_ || frame_.width.unit != Unit::Pixels || frame_.height.unit != Unit::Pixels ? preferredSize() : Size{0,0};
    if (hasFrame_ && frame_.width.unit == Unit::Pixels) result.width = nonnegative(frame_.width.value);
    if (hasFrame_ && frame_.height.unit == Unit::Pixels) result.height = nonnegative(frame_.height.value);
    return result;
  }

  bool widthIsMax() const {
    return widthModeSet_ ? widthMode_ == SizeMode::Max : (hasFrame_ && frame_.width.unit == Unit::Max);
  }

  bool heightIsMax() const {
    return heightModeSet_ ? heightMode_ == SizeMode::Max : (hasFrame_ && frame_.height.unit == Unit::Max);
  }

  virtual Size preferredSize() const { return {0,0}; }
  virtual void layout(Rect parent) {
    const Size wanted = requestedSize(parent);
    bounds_ = {int16_t(parent.x+(hasFrame_ ? resolveLength(frame_.x,parent.width) : 0)),
               int16_t(parent.y+(hasFrame_ ? resolveLength(frame_.y,parent.height) : 0)),
               widthIsMax() ? parent.width : wanted.width, heightIsMax() ? parent.height : wanted.height};
  }
  virtual void update(uint32_t /* nowMs */) {}
  virtual void draw(Canvas& canvas) const = 0;

  // Interactive components opt in to focus navigation and actions.
  virtual bool interactive() const { return false; }
  virtual bool focused() const { return false; }
  virtual Rect focusBounds() const { return bounds_; }
  virtual uint8_t focusCornerCut() const { return 0; }
  virtual void setFocused(bool) {}
  virtual bool handle(Action) { return false; }
  virtual bool scrollAt(Point, Point) { return false; }
  virtual Rect contentExtent() const { return visible_ ? bounds_ : Rect{0,0,0,0}; }
  virtual bool ensureVisible(const Component&) { return false; }
  virtual bool focusClip(const Component& target, Rect&) const { return this == &target; }
  virtual void setScrollAnimation(uint16_t) {}
  virtual bool locate(const Component& target, Rect& result, bool /* destination */ = false) const {
    if (this != &target) return false;
    result = bounds_; return true;
  }

  virtual void collectFocusables(Component** result, uint8_t& count, uint8_t capacity) {
    if (visible_ && enabled_ && interactive() && count < capacity) result[count++] = this;
  }

 protected:
  Frame frame_;
  Rect bounds_;
  bool hasFrame_;
  bool buildOK_ = true;
  bool visible_ = true;
  bool enabled_ = true;
  SizeMode widthMode_ = SizeMode::Fit;
  SizeMode heightMode_ = SizeMode::Fit;
  bool widthModeSet_ = false;
  bool heightModeSet_ = false;

  // Parent Layout uses this after it changes the flow-axis coordinate.
  void setResolvedBounds(Rect bounds) { bounds_ = bounds; }
  virtual void layoutChildren() {}
  friend class Layout;
  friend class GridLayout;
  friend class ContentControl;
  friend class Page;

  static int16_t nonnegative(int16_t value) { return value < 0 ? 0 : value; }

  static int16_t resolveLength(Length length, int16_t reference) {
    if (length.unit == Unit::Auto) return 0;
    if (length.unit == Unit::Max) return reference;
    if (length.unit == Unit::Pixels) return length.value;
    const int32_t result = static_cast<int32_t>(reference) * length.value / 100;
    return result > 32767 ? 32767 : (result < -32768 ? -32768 : static_cast<int16_t>(result));
  }

  static Rect resolveFrame(Frame frame, Rect parent) {
    return {
        static_cast<int16_t>(parent.x + resolveLength(frame.x, parent.width)),
        static_cast<int16_t>(parent.y + resolveLength(frame.y, parent.height)),
        resolveLength(frame.width, parent.width),
        resolveLength(frame.height, parent.height)};
  }
};

// An lvalue is borrowed; an rvalue is retained by the tree.
class ComponentSlot {
 public:
  operator Component*() const { return component_; }
  Component* operator->() const { return component_; }
  ComponentSlot& operator=(Component* component) { owner_.reset(); component_ = component; return *this; }
  template<class T> bool own(T&& component) {
    using Value = detail::SharedValue<detail::Stored<T>>;
    Value* storage = new (std::nothrow) Value(detail::forward<T>(component));
    if (!storage) return false;
    owner_.reset(storage); component_ = &storage->value; return true;
  }
 private:
  Component* component_ = nullptr;
  detail::SharedOwner owner_;
};

#define MAD_DETAIL_LAYOUT_CHAIN(Type, Base) \
  MAD_DETAIL_COMPONENT_CHAIN(Type) \
  MAD_DETAIL_CHAIN_METHOD(Type, Base, setScrollable, bool) \
  MAD_DETAIL_CHAIN_METHOD(Type, Base, setChildWidthMode, SizeMode) \
  MAD_DETAIL_CHAIN_METHOD(Type, Base, setChildHeightMode, SizeMode) \
  using Base::add; \
  template<class... Children> Type&& add(Children&&... children) && { \
    this->appendChildren(detail::forward<Children>(children)...); return static_cast<Type&&>(*this); } \
  template<class First,class Second,class... Rest> Type& add(First&& first,Second&& second,Rest&&... rest) & { \
    this->appendChildren(detail::forward<First>(first),detail::forward<Second>(second),detail::forward<Rest>(rest)...); return *this; }

enum class LayoutDirection : uint8_t { Vertical, Horizontal };

// A Layout is a normal component. Every Page receives an explicit root layout.
class Layout : public Component {
 public:
  static constexpr uint8_t kMaxComponents = 16;

  explicit Layout(LayoutDirection direction) : Component(), direction_(direction) {}
  Layout(Frame frame, LayoutDirection direction) : Component(frame), direction_(direction) {}

  bool add(Component& component) & {
    Rect descendant;
    if (&component == this || count_ == kMaxComponents || component.locate(*this,descendant)) return false;
    for (uint8_t i = 0; i < count_; ++i) if (children_[i] == &component) return false;
    component.setScrollAnimation(scrollDurationMs_);
    children_[count_++] = &component;
    return true;
  }

  template<class T> typename detail::EnableIf<!detail::IsLvalue<T>::value,bool>::Type add(T&& component) & {
    ComponentSlot slot;
    if (!slot.own(detail::forward<T>(component)) || !add(*static_cast<Component*>(slot))) return false;
    children_[count_-1] = slot;
    return true;
  }
  bool valid() const override {
    if (!Component::valid()) return false;
    for (uint8_t i = 0; i < count_; ++i) if (!children_[i]->valid()) return false;
    return true;
  }

  LayoutDirection direction() const { return direction_; }

  void setScrollable(bool enabled) {
    scrollable_ = enabled;
    if (!enabled) { scrollOffset_ = scrollTarget_ = 0; scrollMotion_.start(0,0,nowMs_,0); }
    else {
      if (direction_ == LayoutDirection::Vertical && (!hasFrame_ || frame_.height.unit == Unit::Auto) && !hasHeightMode()) setHeightMode(SizeMode::Max);
      if (direction_ == LayoutDirection::Horizontal && (!hasFrame_ || frame_.width.unit == Unit::Auto) && !hasWidthMode()) setWidthMode(SizeMode::Max);
    }
  }
  bool scrollable() const { return scrollable_; }
  int16_t scrollOffset() const { return scrollOffset_; }
  int16_t scrollTarget() const { return scrollTarget_; }
  int16_t scrollContentExtent() const { return contentMain_; }
  Rect scrollViewport() const { return scrollContentBounds(); }
  void setScrollOffset(int16_t offset) {
    if (!scrollable_) {
      scrollOffset_ = 0;
      return;
    }
    scrollOffset_ = scrollTarget_ = offset < 0 ? 0 : offset;
    if (bounds_.width > 0 && bounds_.height > 0) layoutChildren();
    scrollTarget_ = scrollOffset_;
    scrollMotion_.start(scrollOffset_,scrollOffset_,nowMs_,0);
  }
  void scrollBy(int16_t delta) {
    const int32_t next = static_cast<int32_t>(scrollOffset_) + delta;
    setScrollOffset(next < 0 ? 0 : next > 32767 ? 32767 : static_cast<int16_t>(next));
  }
  void scrollBy(LayoutDirection direction, int16_t delta) {
    if (direction == direction_) scrollBy(delta);
  }

  bool scrollAt(Point position, Point delta) override {
    if (!visible_ || !containsPoint(bounds_,position)) return false;
    for (uint8_t i = count_; i > 0; --i)
      if (children_[i-1]->visible() && children_[i-1]->scrollAt(position,delta)) return true;
    const int16_t amount = direction_ == LayoutDirection::Vertical ? delta.y : delta.x;
    return scrollable_ && amount && scrollTo(static_cast<int32_t>(scrollTarget_)+amount);
  }
  Rect contentExtent() const override {
    if (!visible_) return {0,0,0,0};
    if (scrollable_) return bounds_;
    Rect result{bounds_.x,bounds_.y,0,0};
    for (uint8_t i = 0; i < count_; ++i)
      if (children_[i]->visible()) result = unionBounds(result,children_[i]->contentExtent());
    return result;
  }

  // One pixel is the common spacing between flow children. A zero gap is
  // still available for deliberately dense layouts.
  void setGap(uint8_t gap) { gap_ = gap; }
  uint8_t gap() const { return gap_; }

  // Optional defaults for every child. A child-level setWidthMode/setHeightMode
  // always wins. Unspecified no-frame child dimensions default to Fit; use
  // Max explicitly when the child should consume available space.
  void setChildWidthMode(SizeMode mode) { childWidthMode_ = mode; childWidthModeSet_ = true; }
  void setChildHeightMode(SizeMode mode) { childHeightMode_ = mode; childHeightModeSet_ = true; }
  SizeMode childWidthMode() const { return childWidthMode_; }
  SizeMode childHeightMode() const { return childHeightMode_; }

  Size preferredSize() const override {
    Size result{0, 0};
    uint8_t visibleCount = 0;
    for (uint8_t i = 0; i < count_; ++i) {
      if (!children_[i]->visible()) continue;
      ++visibleCount;
      const Size child = children_[i]->intrinsicSize();
      if (direction_ == LayoutDirection::Vertical) {
        result.width = maximum(result.width, child.width);
        result.height = saturatingAdd(result.height, child.height);
      } else {
        result.width = saturatingAdd(result.width, child.width);
        result.height = maximum(result.height, child.height);
      }
    }
    if (visibleCount > 1) {
      const int16_t spacing = static_cast<int16_t>(gap_) * static_cast<int16_t>(visibleCount - 1);
      if (direction_ == LayoutDirection::Vertical) result.height = saturatingAdd(result.height, spacing);
      else result.width = saturatingAdd(result.width, spacing);
    }
    return result;
  }

  void layout(Rect parent) override {
    Component::layout(parent);
    layoutChildren();
  }

  void draw(Canvas& canvas) const override {
    if (!visible_) return;
    drawChildren(canvas);
    drawScrollBar(canvas);
  }

  void setScrollAnimation(uint16_t durationMs) override {
    scrollDurationMs_ = durationMs;
    if (!durationMs) { scrollOffset_ = scrollTarget_; scrollMotion_.start(scrollOffset_,scrollOffset_,nowMs_,0); }
    for (uint8_t i = 0; i < count_; ++i) children_[i]->setScrollAnimation(durationMs);
  }
  bool focusClip(const Component& target, Rect& clip) const override {
    if (this == &target) return true;
    for (uint8_t i = 0; i < count_; ++i) if (children_[i]->focusClip(target,clip)) {
      if (scrollable_) clip = intersectBounds(clip,scrollContentBounds());
      return true;
    }
    return false;
  }
  bool locate(const Component& target, Rect& result, bool destination = false) const override {
    if (Component::locate(target,result,destination)) return true;
    for (uint8_t i = 0; i < count_; ++i) if (children_[i]->locate(target,result,destination)) {
      if (destination) {
        if (direction_ == LayoutDirection::Vertical) result.y -= scrollTarget_ - scrollOffset_;
        else result.x -= scrollTarget_ - scrollOffset_;
      }
      return true;
    }
    return false;
  }
  void update(uint32_t nowMs) override {
    nowMs_ = nowMs; timed_ = true;
    const int16_t next = static_cast<int16_t>(scrollMotion_.value(nowMs) + .5f);
    if (next != scrollOffset_) { scrollOffset_ = next; layoutChildren(); }
    for (uint8_t i = 0; i < count_; ++i) if (children_[i]->visible()) children_[i]->update(nowMs);
  }

  void collectFocusables(Component** result, uint8_t& count, uint8_t capacity) override {
    if (!visible_ || !enabled_) return;
    for (uint8_t i = 0; i < count_; ++i) children_[i]->collectFocusables(result, count, capacity);
  }

  bool ensureVisible(const Component& target) override {
    for (uint8_t i = 0; i < count_; ++i) {
      Component* child = children_[i];
      if (child == &target || child->ensureVisible(target)) {
        Rect destination = target.bounds();
        child->locate(target,destination,true);
        Rect clip = unionBounds(destination,child->contentExtent()); child->focusClip(target,clip);
        adjustScrollTo(intersectBounds(destination,clip));
        return true;
      }
    }
    return false;
  }

 protected:
  void appendChildren() {}
  template<class First,class... Rest> void appendChildren(First&& first,Rest&&... rest) {
    if (!add(detail::forward<First>(first))) { buildOK_ = false; return; }
    appendChildren(detail::forward<Rest>(rest)...);
  }
  virtual Rect contentBounds() const { return bounds_; }

  void drawChildren(Canvas& canvas) const {
    Rect clip = scrollContentBounds();
    CanvasView clipped(canvas,clip);
    for (uint8_t i = 0; i < count_; ++i) {
      if (!children_[i]->visible() || (scrollable_ && !intersects(children_[i]->contentExtent(), clip))) continue;
      children_[i]->draw(scrollable_ ? static_cast<Canvas&>(clipped) : canvas);
    }
  }

  void layoutChildren() override {
    if (count_ == 0) {
      contentMain_ = 0;
      scrollOffset_ = scrollTarget_ = 0; scrollMotion_.start(0,0,nowMs_,0);
      return;
    }
    const Rect inner = contentBounds();
    const bool vertical = direction_ == LayoutDirection::Vertical;
    const int16_t availableMain = vertical ? inner.height : inner.width;
    int16_t availableCross = vertical ? inner.width : inner.height;
    uint8_t visibleCount = 0;
    for (uint8_t i = 0; i < count_; ++i) if (children_[i]->visible()) ++visibleCount;
    const int16_t spacing = visibleCount > 1 ? static_cast<int16_t>(gap_) * (visibleCount - 1) : 0;
    const int16_t innerMain = availableMain > spacing ? static_cast<int16_t>(availableMain - spacing) : 0;
    Size requested[kMaxComponents]{};
    bool mainMax[kMaxComponents]{};
    int16_t fixedMain = 0;
    uint8_t maxCount = 0;

    for (uint8_t i = 0; i < count_; ++i) {
      Component* child = children_[i];
      if (!child->visible()) continue;
      requested[i] = child->requestedSize(inner);
      mainMax[i] = isMainMax(*child);
      if (mainMax[i]) ++maxCount;
      else fixedMain = saturatingAdd(fixedMain, vertical ? requested[i].height : requested[i].width);
    }

    const int16_t intrinsicMain = saturatingAdd(fixedMain, spacing);
    contentMain_ = maxCount && intrinsicMain < availableMain ? availableMain : intrinsicMain;
    // Measure descendant overflow before clamping; the second pass places the
    // children with the resulting offset and scrollbar lane.
    for (uint8_t pass = 0; pass < 2; ++pass) {
      availableCross = vertical ? inner.width : inner.height;
      const bool overflow = scrollable_ && contentMain_ > availableMain;
      if (overflow) availableCross = availableCross > 3 ? availableCross-3 : 0;
      if (!scrollable_) scrollOffset_ = 0;
      int16_t remaining = innerMain > fixedMain ? static_cast<int16_t>(innerMain - fixedMain) : 0;
      int16_t cursor = static_cast<int16_t>((vertical ? inner.y : inner.x) - scrollOffset_);
      uint8_t remainingMax = maxCount;
      for (uint8_t i = 0; i < count_; ++i) {
        Component* child = children_[i];
        if (!child->visible()) { child->setResolvedBounds({0, 0, 0, 0}); continue; }
        const bool crossMax = isCrossMax(*child);
        const int16_t main = mainMax[i]
            ? (remainingMax == 1 ? remaining : static_cast<int16_t>(remaining / remainingMax))
            : (vertical ? requested[i].height : requested[i].width);
        if (mainMax[i]) {
          remaining = static_cast<int16_t>(remaining - main);
          --remainingMax;
        }
        const int16_t cross = crossMax ? availableCross : (vertical ? requested[i].width : requested[i].height);
        Rect childBounds = vertical
            ? Rect{inner.x, cursor, cross, main}
            : Rect{cursor, inner.y, main, cross};
        child->setResolvedBounds(childBounds);
        child->layoutChildren();
        cursor = saturatingAdd(cursor, main);
        if (i + 1 < count_) cursor = saturatingAdd(cursor, gap_);
      }
      int16_t measured = maxCount && intrinsicMain < availableMain ? availableMain : intrinsicMain;
      if (scrollable_) for (uint8_t i = 0; i < count_; ++i) if (children_[i]->visible()) {
        const Rect extent = children_[i]->contentExtent();
        const int32_t end = vertical ? int32_t(extent.y)+extent.height-inner.y+scrollOffset_ : int32_t(extent.x)+extent.width-inner.x+scrollOffset_;
        measured = maximum(measured, end > 32767 ? 32767 : end < 0 ? 0 : int16_t(end));
      }
      const int16_t previousOffset = scrollOffset_;
      contentMain_ = measured;
      clampScrollOffset();
      if (previousOffset == scrollOffset_ && overflow == (scrollable_ && contentMain_ > availableMain)) break;
    }
  }

  Rect scrollContentBounds() const {
    Rect inner = contentBounds();
    const int16_t length = direction_ == LayoutDirection::Vertical ? inner.height : inner.width;
    if (scrollable_ && contentMain_ > length) {
      if (direction_ == LayoutDirection::Vertical) inner.width = inner.width > 3 ? inner.width-3 : 0;
      else inner.height = inner.height > 3 ? inner.height-3 : 0;
    }
    return inner;
  }
  void drawScrollBar(Canvas& canvas) const {
    const Rect inner = contentBounds();
    const bool vertical = direction_ == LayoutDirection::Vertical;
    const int16_t length = vertical ? inner.height : inner.width;
    const int16_t cross = vertical ? inner.width : inner.height;
    if (!scrollable_ || contentMain_ <= length || length <= 0 || cross < 3) return;
    int16_t thumb = static_cast<int32_t>(length)*length/contentMain_;
    if (thumb < 2) thumb = length < 2 ? length : 2;
    const int16_t start = static_cast<int32_t>(scrollOffset_)*(length-thumb)/(contentMain_-length);
    for (int16_t main = 0; main < length; ++main) for (int16_t lane = 0; lane < 3; ++lane) {
      const bool on = (lane > 0 && main >= start && main < start+thumb) || (lane == 2 && main%2 == 0);
      canvas.fixedPixel(static_cast<int16_t>(inner.x+(vertical ? inner.width-3+lane : main)),
                        static_cast<int16_t>(inner.y+(vertical ? main : inner.height-3+lane)), on ? Color::On : Color::Off);
    }
  }

  private:
  friend class GridLayout;
  bool scrollTo(int32_t next) {
    const Rect inner = contentBounds();
    const int16_t length = direction_ == LayoutDirection::Vertical ? inner.height : inner.width;
    const int16_t maximum = contentMain_ > length ? contentMain_-length : 0;
    next = next < 0 ? 0 : (next > maximum ? maximum : next);
    if (next == scrollTarget_) return false;
    scrollTarget_ = static_cast<int16_t>(next);
    if (timed_ && scrollDurationMs_) scrollMotion_.retarget(scrollTarget_,nowMs_,scrollDurationMs_);
    else { scrollOffset_ = scrollTarget_; scrollMotion_.start(scrollOffset_,scrollOffset_,nowMs_,0); layoutChildren(); }
    return true;
  }
  void clampScrollOffset() {
    const Rect inner = contentBounds();
    const int16_t viewport = direction_ == LayoutDirection::Vertical ? inner.height : inner.width;
    const int16_t maximum = scrollable_ && contentMain_ > viewport ? static_cast<int16_t>(contentMain_ - viewport) : 0;
    if (scrollOffset_ > maximum) scrollOffset_ = maximum;
    if (scrollTarget_ > maximum) {
      scrollTarget_ = maximum;
      scrollMotion_.start(scrollOffset_,scrollTarget_,nowMs_,timed_ ? scrollDurationMs_ : 0);
    }
  }

  void adjustScrollTo(Rect child) {
    if (!scrollable_) return;
    const Rect inner = contentBounds();
    const int32_t viewStart = direction_ == LayoutDirection::Vertical ? inner.y : inner.x;
    const int32_t viewport = direction_ == LayoutDirection::Vertical ? inner.height : inner.width;
    const int32_t start = (direction_ == LayoutDirection::Vertical ? child.y : child.x) + scrollOffset_;
    const int32_t length = direction_ == LayoutDirection::Vertical ? child.height : child.width;
    const int32_t lower = start + length - viewStart - viewport;
    const int32_t upper = start - viewStart;
    int32_t next = scrollTarget_;
    if (length > viewport) next = upper;
    else { if (next < lower) next = lower; if (next > upper) next = upper; }
    scrollTo(next);
  }

  static bool intersects(Rect a, Rect b) {
    return a.x < b.x + b.width && b.x < a.x + a.width &&
           a.y < b.y + b.height && b.y < a.y + a.height;
  }

  bool childWidthIsMax(const Component& child) const {
    if (child.hasWidthMode()) return child.widthMode() == SizeMode::Max;
    if (child.hasFrame()) return child.widthIsMax();
    if (childWidthModeSet_) return childWidthMode_ == SizeMode::Max;
    return false;
  }

  bool childHeightIsMax(const Component& child) const {
    if (child.hasHeightMode()) return child.heightMode() == SizeMode::Max;
    if (child.hasFrame()) return child.heightIsMax();
    if (childHeightModeSet_) return childHeightMode_ == SizeMode::Max;
    return false;
  }

  bool isMainMax(const Component& child) const {
    return direction_ == LayoutDirection::Vertical ? childHeightIsMax(child) : childWidthIsMax(child);
  }

  bool isCrossMax(const Component& child) const {
    return direction_ == LayoutDirection::Vertical ? childWidthIsMax(child) : childHeightIsMax(child);
  }

  static int16_t maximum(int16_t a, int16_t b) { return a > b ? a : b; }
  static int16_t saturatingAdd(int16_t a, int16_t b) {
    const int32_t sum = static_cast<int32_t>(a) + b;
    return sum > 32767 ? 32767 : (sum < -32768 ? -32768 : static_cast<int16_t>(sum));
  }

  ComponentSlot children_[kMaxComponents]{};
  LayoutDirection direction_;
  uint8_t count_ = 0;
  uint8_t gap_ = kDefaultGap;
  bool scrollable_ = false;
  int16_t scrollOffset_ = 0;
  int16_t scrollTarget_ = 0;
  Tween scrollMotion_;
  uint32_t nowMs_ = 0;
  uint16_t scrollDurationMs_ = 280;
  bool timed_ = false;
  int16_t contentMain_ = 0;
  SizeMode childWidthMode_ = SizeMode::Fit;
  SizeMode childHeightMode_ = SizeMode::Fit;
  bool childWidthModeSet_ = false;
  bool childHeightModeSet_ = false;
};

// Row-major grid. Equal column tracks, intrinsic row heights, optional vertical scrolling.
class GridLayout final : public Layout {
 public:
  MAD_DETAIL_LAYOUT_CHAIN(GridLayout, Layout)
  explicit GridLayout(uint8_t columns = 2) : Layout(LayoutDirection::Vertical) { setColumns(columns); }
  GridLayout(Frame frame,uint8_t columns = 2) : Layout(frame,LayoutDirection::Vertical) { setColumns(columns); }
  GridLayout& setColumns(uint8_t columns) & { columns_ = columns < 1 ? 1 : columns > kMaxComponents ? kMaxComponents : columns; return *this; }
  GridLayout&& setColumns(uint8_t columns) && { setColumns(columns); return static_cast<GridLayout&&>(*this); }
  uint8_t columns() const { return columns_; }
  GridLayout& setGap(uint8_t gap) & { Layout::setGap(gap); columnGap_ = gap; return *this; }
  GridLayout&& setGap(uint8_t gap) && { setGap(gap); return static_cast<GridLayout&&>(*this); }
  GridLayout& setRowGap(uint8_t gap) & { Layout::setGap(gap); return *this; }
  GridLayout&& setRowGap(uint8_t gap) && { setRowGap(gap); return static_cast<GridLayout&&>(*this); }
  GridLayout& setColumnGap(uint8_t gap) & { columnGap_ = gap; return *this; }
  GridLayout&& setColumnGap(uint8_t gap) && { setColumnGap(gap); return static_cast<GridLayout&&>(*this); }
  uint8_t rowGap() const { return gap(); }
  uint8_t columnGap() const { return columnGap_; }

  Size preferredSize() const override {
    int16_t width = 0,heights[kMaxComponents]{}; uint8_t visible = 0;
    for (uint8_t i = 0; i < count_; ++i) if (children_[i]->visible()) {
      const Size size = children_[i]->intrinsicSize();
      width = maximum(width,size.width);
      const uint8_t row = visible++/columns_;
      heights[row] = maximum(heights[row],size.height);
    }
    if (!visible) return {0,0};
    const uint8_t rows = (visible+columns_-1)/columns_;
    int32_t height = int32_t(rows-1)*gap_;
    for (uint8_t row = 0; row < rows; ++row) height += heights[row];
    return {length(int32_t(width)*columns_+int32_t(columns_-1)*columnGap_),length(height)};
  }
  void draw(Canvas& canvas) const override {
    if (!visible_) return;
    const bool constrainWidth = widthIsMax() || (hasFrame_ && frame_.width.unit != Unit::Auto);
    CanvasView viewport(canvas,scrollable_ ? scrollContentBounds() : canvas.viewport());
    for (uint8_t i = 0; i < count_; ++i) if (children_[i]->visible()) {
      CanvasView cell(viewport,children_[i]->bounds());
      children_[i]->draw(constrainWidth ? static_cast<Canvas&>(cell) : static_cast<Canvas&>(viewport));
    }
    drawScrollBar(canvas);
  }

 protected:
  void layoutChildren() override {
    uint8_t visible = 0;
    for (uint8_t i = 0; i < count_; ++i) if (children_[i]->visible()) ++visible;
    if (!visible) { contentMain_ = scrollOffset_ = scrollTarget_ = 0; scrollMotion_.start(0,0,nowMs_,0); return; }
    const uint8_t rows = (visible+columns_-1)/columns_;
    const Rect inner = contentBounds();
    int16_t heights[kMaxComponents]{},widths[kMaxComponents]{};
    Size requested[kMaxComponents]{}; bool grow[kMaxComponents]{};
    uint8_t index = 0,growingRows = 0;
    const Rect cell{0,0,length((int32_t(inner.width)-int32_t(columns_-1)*columnGap_)/columns_),inner.height};
    for (uint8_t i = 0; i < count_; ++i) {
      Component& child = *children_[i];
      if (!child.visible()) { child.setResolvedBounds({0,0,0,0}); continue; }
      requested[i] = child.requestedSize(cell);
      const Size intrinsic = child.preferredSize();
      if (childWidthIsMax(child)) requested[i].width = intrinsic.width;
      if (childHeightIsMax(child)) requested[i].height = intrinsic.height;
      const uint8_t row = index++/columns_;
      heights[row] = maximum(heights[row],requested[i].height);
      if (childHeightIsMax(child) && !grow[row]) { grow[row] = true; ++growingRows; }
    }
    int32_t naturalHeight = int32_t(rows-1)*gap_;
    for (uint8_t row = 0; row < rows; ++row) naturalHeight += heights[row];
    int16_t extraHeight = length(int32_t(inner.height)-naturalHeight);
    for (uint8_t row = 0; row < rows; ++row) if (grow[row]) {
      const int16_t extra = extraHeight/growingRows--;
      heights[row] = length(int32_t(heights[row])+extra); extraHeight -= extra;
    }
    int32_t contentHeight = int32_t(rows-1)*gap_;
    for (uint8_t row = 0; row < rows; ++row) contentHeight += heights[row];
    contentMain_ = length(contentHeight);
    const int16_t availableWidth = length(int32_t(inner.width)-(scrollable_ && contentMain_ > inner.height ? 3 : 0));
    const Rect visibleCell{0,0,length((int32_t(availableWidth)-int32_t(columns_-1)*columnGap_)/columns_),inner.height};
    for (uint8_t i = 0; i < count_; ++i) if (children_[i]->visible()) {
      requested[i].width = childWidthIsMax(*children_[i]) ? children_[i]->preferredSize().width : children_[i]->requestedSize(visibleCell).width;
    }
    int16_t remainingWidth = length(int32_t(availableWidth)-int32_t(columns_-1)*columnGap_);
    for (uint8_t col = 0; col < columns_; ++col) {
      widths[col] = remainingWidth/(columns_-col); remainingWidth -= widths[col];
    }
    clampScrollOffset();
    const bool constrainWidth = widthIsMax() || (hasFrame_ && frame_.width.unit != Unit::Auto);
    int16_t x = inner.x,y = saturatingAdd(inner.y,-scrollOffset_); index = 0;
    for (uint8_t i = 0; i < count_; ++i) if (children_[i]->visible()) {
      Component& child = *children_[i]; const uint8_t row = index/columns_,col = index%columns_;
      child.setResolvedBounds({x,y,childWidthIsMax(child) || (constrainWidth && requested[i].width > widths[col]) ? widths[col] : length(requested[i].width),
          childHeightIsMax(child) ? heights[row] : length(requested[i].height)});
      child.layoutChildren(); ++index;
      if (col+1 == columns_) { x = inner.x; y = saturatingAdd(y,saturatingAdd(heights[row],gap_)); }
      else x = saturatingAdd(x,saturatingAdd(widths[col],columnGap_));
    }
  }
 private:
  static int16_t length(int32_t value) { return value < 0 ? 0 : value > 32767 ? 32767 : static_cast<int16_t>(value); }
  uint8_t columns_ = 2,columnGap_ = kDefaultGap;
};

class VerticalLayout final : public Layout {
 public:
  MAD_DETAIL_LAYOUT_CHAIN(VerticalLayout, Layout)
  MAD_DETAIL_CHAIN_METHOD(VerticalLayout, Layout, setGap, uint8_t)
  VerticalLayout() : Layout(LayoutDirection::Vertical) {}
  explicit VerticalLayout(Frame frame) : Layout(frame, LayoutDirection::Vertical) {}
};

// Container is a visual-less vertical wrapper for grouping components.
class Container : public Layout {
 public:
  MAD_DETAIL_LAYOUT_CHAIN(Container, Layout)
  MAD_DETAIL_CHAIN_METHOD(Container, Layout, setGap, uint8_t)
  Container() : Layout(LayoutDirection::Vertical) {}
  explicit Container(Frame frame) : Layout(frame, LayoutDirection::Vertical) {}
};

// Panel is a Container with a one-pixel rectangular border and configurable
// background. Its children are laid out inside the border.
class Panel final : public Container {
 public:
  MAD_DETAIL_LAYOUT_CHAIN(Panel, Container)
  MAD_DETAIL_CHAIN_METHOD(Panel, Container, setGap, uint8_t)
  static constexpr int16_t kBorderWidth = 1;
  static constexpr int16_t kContentGap = 1;
  static constexpr int16_t kContentInset = kBorderWidth + kContentGap;

  explicit Panel(Color border = Color::On, Color background = Color::Off)
      : Container(), border_(border), background_(background) {}
  Panel(Frame frame, Color border = Color::On, Color background = Color::Off)
      : Container(frame), border_(border), background_(background) {}

  void setBorderColor(Color color) { border_ = color; }
  Color borderColor() const { return border_; }
  void setBackground(Color color) { background_ = color; }
  Color background() const { return background_; }

  Rect contentExtent() const override {
    return visible_ ? unionBounds(bounds_,Layout::contentExtent()) : Rect{0,0,0,0};
  }

  Size preferredSize() const override {
    const Size content = Layout::preferredSize();
    return {saturatingAdd(content.width, static_cast<int16_t>(kContentInset * 2)),
            saturatingAdd(content.height, static_cast<int16_t>(kContentInset * 2))};
  }

  void draw(Canvas& canvas) const override {
    if (!visible_) return;
    canvas.fillRect(bounds_, background_);
    canvas.strokeRect(bounds_, border_);
    CanvasView content(canvas, contentBounds());
    drawChildren(scrollable() ? static_cast<Canvas&>(content) : canvas);
    drawScrollBar(content);
  }

 protected:
  Rect contentBounds() const override {
    return {static_cast<int16_t>(bounds_.x + kContentInset), static_cast<int16_t>(bounds_.y + kContentInset),
        static_cast<int16_t>(bounds_.width > 2 * kContentInset ? bounds_.width - 2 * kContentInset : 0),
        static_cast<int16_t>(bounds_.height > 2 * kContentInset ? bounds_.height - 2 * kContentInset : 0)};
  }

 private:
  static int16_t saturatingAdd(int16_t a, int16_t b) {
    const int32_t sum = static_cast<int32_t>(a) + b;
    return sum > 32767 ? 32767 : static_cast<int16_t>(sum);
  }

  Color border_;
  Color background_;
};

class HorizontalLayout final : public Layout {
 public:
  MAD_DETAIL_LAYOUT_CHAIN(HorizontalLayout, Layout)
  MAD_DETAIL_CHAIN_METHOD(HorizontalLayout, Layout, setGap, uint8_t)
  HorizontalLayout() : Layout(LayoutDirection::Horizontal) {}
  explicit HorizontalLayout(Frame frame) : Layout(frame, LayoutDirection::Horizontal) {}
};

// FreeLayout is the explicit-position panel. It is the only container where
// child x/y are honored; percentages resolve against the panel's own bounds.
class FreeLayout final : public Component {
 public:
  MAD_DETAIL_COMPONENT_CHAIN(FreeLayout)
  template<class... Children> FreeLayout&& add(Children&&... children) && {
    appendChildren(detail::forward<Children>(children)...); return static_cast<FreeLayout&&>(*this);
  }
  template<class First,class Second,class... Rest> auto add(First&& first,Second&& second,Rest&&... rest) & -> decltype(static_cast<Component&>(second),*this) {
    appendChildren(detail::forward<First>(first),detail::forward<Second>(second),detail::forward<Rest>(rest)...); return *this;
  }

  static constexpr uint8_t kMaxComponents = 16;

  explicit FreeLayout(Frame frame = Frame::percent(0,0,100,100)) : Component(frame) {}

  bool add(Component& component, Frame frame) & {
    Rect descendant;
    if (&component == this || count_ == kMaxComponents || component.locate(*this,descendant)) return false;
    for (uint8_t i = 0; i < count_; ++i) if (children_[i] == &component) return false;
    component.setFrame(frame);
    component.setScrollAnimation(scrollDurationMs_);
    children_[count_++] = &component;
    return true;
  }

  FreeLayout&& add(Component& component,Frame frame) && {
    if (!static_cast<FreeLayout&>(*this).add(component,frame)) buildOK_ = false;
    return static_cast<FreeLayout&&>(*this);
  }
  template<class T> bool add(T&& component,Frame frame) & {
    component.setFrame(frame); return add(detail::forward<T>(component));
  }
  template<class T> FreeLayout&& add(T&& component,Frame frame) && {
    if (!static_cast<FreeLayout&>(*this).add(detail::forward<T>(component),frame)) buildOK_ = false;
    return static_cast<FreeLayout&&>(*this);
  }

  bool add(Component& component) & {
    Rect descendant;
    if (&component == this || count_ == kMaxComponents || component.locate(*this,descendant)) return false;
    for (uint8_t i = 0; i < count_; ++i) if (children_[i] == &component) return false;
    component.setScrollAnimation(scrollDurationMs_);
    children_[count_++] = &component;
    return true;
  }

  template<class T> typename detail::EnableIf<!detail::IsLvalue<T>::value,bool>::Type add(T&& component) & {
    ComponentSlot slot;
    if (!slot.own(detail::forward<T>(component)) || !add(*static_cast<Component*>(slot))) return false;
    children_[count_-1] = slot; return true;
  }
  bool valid() const override {
    if (!Component::valid()) return false;
    for (uint8_t i = 0; i < count_; ++i) if (!children_[i]->valid()) return false;
    return true;
  }

  void layout(Rect parent) override {
    Component::layout(parent);
    layoutChildren();
  }

  void draw(Canvas& canvas) const override {
    if (!visible_) return;
    for (uint8_t i = 0; i < count_; ++i)
      if (children_[i]->visible()) children_[i]->draw(canvas);
  }

  bool scrollAt(Point position, Point delta) override {
    if (!visible_) return false;
    for (uint8_t i = count_; i > 0; --i) if (children_[i-1]->scrollAt(position,delta)) return true;
    return false;
  }
  Rect contentExtent() const override {
    if (!visible_) return {0,0,0,0};
    Rect result = bounds_;
    for (uint8_t i = 0; i < count_; ++i) if (children_[i]->visible()) result = unionBounds(result,children_[i]->contentExtent());
    return result;
  }
  bool ensureVisible(const Component& target) override {
    for (uint8_t i = 0; i < count_; ++i)
      if (children_[i] == &target || children_[i]->ensureVisible(target)) return true;
    return false;
  }
  void setScrollAnimation(uint16_t durationMs) override {
    scrollDurationMs_ = durationMs;
    for (uint8_t i = 0; i < count_; ++i) children_[i]->setScrollAnimation(durationMs);
  }
  bool focusClip(const Component& target, Rect& clip) const override {
    if (this == &target) return true;
    for (uint8_t i = 0; i < count_; ++i) if (children_[i]->focusClip(target,clip)) {
      return true;
    }
    return false;
  }
  bool locate(const Component& target, Rect& result, bool destination = false) const override {
    if (Component::locate(target,result,destination)) return true;
    for (uint8_t i = 0; i < count_; ++i) if (children_[i]->locate(target,result,destination)) return true;
    return false;
  }
  void update(uint32_t nowMs) override {
    for (uint8_t i = 0; i < count_; ++i) if (children_[i]->visible()) children_[i]->update(nowMs);
  }

  void collectFocusables(Component** result, uint8_t& count, uint8_t capacity) override {
    if (!visible_ || !enabled_) return;
    for (uint8_t i = 0; i < count_; ++i) children_[i]->collectFocusables(result, count, capacity);
  }

 protected:
  void appendChildren() {}
  template<class First,class... Rest> void appendChildren(First&& first,Rest&&... rest) {
    if (!add(detail::forward<First>(first))) { buildOK_ = false; return; }
    appendChildren(detail::forward<Rest>(rest)...);
  }
  void layoutChildren() override {
    for (uint8_t i = 0; i < count_; ++i) children_[i]->layout(bounds_);
  }

 private:
  ComponentSlot children_[kMaxComponents]{};
  uint8_t count_ = 0;
  uint16_t scrollDurationMs_ = 280;
};

// Leading edges arrive before trailing edges, stretching one continuous body.
class LiquidFocus {
 public:
  void reset() { ready_ = false; }
  void follow(Rect target, bool changed, uint32_t nowMs, uint16_t durationMs, uint8_t cornerCut = 0) {
    if (!ready_) { from_ = target; to_ = target; fromCut_ = cut_ = cornerCut; duration_ = 0; ready_ = true; }
    else if (changed) { from_ = sample(nowMs); fromCut_ = cut_; started_ = nowMs; duration_ = durationMs; }
    to_ = target;
    rect_ = sample(nowMs);
    const uint32_t elapsed = nowMs - started_;
    const float t = duration_ && elapsed < duration_ ? static_cast<float>(elapsed) / duration_ : 1;
    cut_ = fromCut_ + (cornerCut - fromCut_) * ease(t,Easing::CubicInOut);
    activity_ = 4 * t * (1 - t);
  }
  Rect bounds() const { return rect_; }
  bool contains(int32_t x, int32_t y) const {
    const float px = x + .5f - rect_.x, py = y + .5f - rect_.y;
    const float w = rect_.width, h = rect_.height;
    if (w <= 0 || h <= 0 || px < 0 || py < 0 || px >= w || py >= h) return false;
    const int32_t ix = x - rect_.x, iy = y - rect_.y;
    const int cut = static_cast<int>(cut_ + .5f);
    if (ix + iy < cut || rect_.width - 1 - ix + iy < cut ||
        ix + rect_.height - 1 - iy < cut || rect_.width - 1 - ix + rect_.height - 1 - iy < cut) return false;
    const float half = (w < h ? w : h) * .5f;
    float radius = activity_ * half;
    if (radius > half) radius = half;
    const float dx = px < radius ? radius - px : (px > w - radius ? px - w + radius : 0);
    const float dy = py < radius ? radius - py : (py > h - radius ? py - h + radius : 0);
    return dx * dx + dy * dy <= radius * radius;
  }
 private:
  static int16_t rounded(float value) {
    if (value > 32767) return 32767;
    if (value < -32768) return -32768;
    return static_cast<int16_t>(value + (value < 0 ? -.5f : .5f));
  }
  Rect sample(uint32_t nowMs) const {
    const uint32_t elapsed = nowMs - started_;
    if (!duration_ || elapsed >= duration_) return to_;
    const float t = static_cast<float>(elapsed) / duration_;
    const float lead = ease(t * 1.35f,Easing::CubicOut);
    const float trail = ease((t - .08f) / .92f,Easing::CubicInOut);
    const bool right = to_.x + to_.width * .5f >= from_.x + from_.width * .5f;
    const bool down = to_.y + to_.height * .5f >= from_.y + from_.height * .5f;
    const float left = from_.x + (to_.x - from_.x) * (right ? trail : lead);
    const float top = from_.y + (to_.y - from_.y) * (down ? trail : lead);
    const float edgeX = static_cast<float>(from_.x) + from_.width + (static_cast<float>(to_.x) + to_.width - from_.x - from_.width) * (right ? lead : trail);
    const float edgeY = static_cast<float>(from_.y) + from_.height + (static_cast<float>(to_.y) + to_.height - from_.y - from_.height) * (down ? lead : trail);
    return {rounded(left),rounded(top),rounded(edgeX > left ? edgeX - left : 1),rounded(edgeY > top ? edgeY - top : 1)};
  }
  Rect from_{0,0,0,0}, to_{0,0,0,0}, rect_{0,0,0,0};
  uint32_t started_ = 0;
  uint16_t duration_ = 0;
  float activity_ = 0;
  float cut_ = 0, fromCut_ = 0;
  bool ready_ = false;
};

// Invert while rasterizing, so the blob reveals the actual glyphs and artwork
// underneath it without a second framebuffer or a readable hardware Canvas.
class LiquidFocusCanvas final : public Canvas {
 public:
  using Canvas::text;
  LiquidFocusCanvas(Canvas& target, const LiquidFocus& blob, Point camera)
      : LiquidFocusCanvas(target,blob,camera,target.viewport()) {}
  LiquidFocusCanvas(Canvas& target, const LiquidFocus& blob, Point camera, Rect clip)
      : target_(target), blob_(blob), camera_(camera), clip_(clip) {}
  Size size() const override { return target_.size(); }
  PixelFormat format() const override { return target_.format(); }
  Rect viewport() const override { return target_.viewport(); }
  void clear(Color color = Color::Off) override { fillRect({0,0,size().width,size().height},color); }
  void fillSpan(int16_t x,int16_t y,int16_t length,Color color) override {
    Rect worldClip = clip_; worldClip.x += camera_.x; worldClip.y += camera_.y;
    const Rect bounds = intersectBounds(blob_.bounds(),worldClip);
    const int32_t worldY = int32_t(y)+camera_.y;
    if (length <= 0) return;
    if (worldY < bounds.y || worldY >= int32_t(bounds.y)+bounds.height ||
        int32_t(x)+length+camera_.x <= bounds.x || int32_t(x)+camera_.x >= int32_t(bounds.x)+bounds.width) {
      target_.fillSpan(x,y,length,color); return;
    }
    int32_t left = int32_t(bounds.x)-camera_.x;
    int32_t right = left+bounds.width;
    if (left < x) left = x;
    if (right > int32_t(x)+length) right = int32_t(x)+length;
    if (left > x) target_.fillSpan(x,y,static_cast<int16_t>(left-x),color);
    int32_t run = left; bool negative = blob_.contains(left+camera_.x,y+camera_.y);
    for (int32_t next = left+1; next <= right; ++next) {
      const bool invert = next < right && blob_.contains(next+camera_.x,y+camera_.y);
      if (next == right || invert != negative) {
        target_.fillSpan(static_cast<int16_t>(run),y,static_cast<int16_t>(next-run),negative ? inverted(color) : color);
        run = next; negative = invert;
      }
    }
    if (right < int32_t(x)+length) target_.fillSpan(static_cast<int16_t>(right),y,static_cast<int16_t>(int32_t(x)+length-right),color);
  }
  void fixedPixel(int16_t x,int16_t y,Color color) override { target_.fixedPixel(x,y,color); }
  void pixel(int16_t x, int16_t y, Color color) override {
    if (containsPoint(clip_,{x,y}) && blob_.contains(static_cast<int32_t>(x) + camera_.x,static_cast<int32_t>(y) + camera_.y))
      color = inverted(color);
    target_.pixel(x,y,color);
  }
  void text(int16_t x,int16_t y,const char* value,uint8_t scale,Color color) override {
    drawDefaultText(*this,x,y,value,scale,color);
  }
  void present() override {}
 private:
  Canvas& target_;
  const LiquidFocus& blob_;
  Point camera_;
  Rect clip_;
};

class Page {
 public:
  // Fixed focus/page capacity keeps MCU memory deterministic.
  static constexpr uint8_t kMaxComponents = 16;

  explicit Page(Color background = Color::Off) : background_(background) {}
  explicit Page(Component& root, Color background = Color::Off) : background_(background) { addRoot(root); }
  template<class T> explicit Page(T&& root, Color background = Color::Off,
      typename detail::EnableIf<!detail::IsLvalue<T>::value>::Type* = nullptr,
      decltype(&static_cast<const Component&>(root)) = nullptr) : background_(background) {
    addRoot(detail::forward<T>(root));
  }
  bool valid() const { return root_ && root_->valid(); }
  Component* root() const { return root_; }
  bool addRoot(Component& root) {
    if (!root.valid()) return false;
    if (root_ == &root) { configureRoot(); return true; }
    resetFocus(); root_ = &root; configureRoot(); return true;
  }
  template<class T> typename detail::EnableIf<!detail::IsLvalue<T>::value,bool>::Type addRoot(T&& root) {
    if (!root.valid()) return false;
    ComponentSlot slot;
    if (!slot.own(detail::forward<T>(root)) || !slot->valid()) return false;
    resetFocus(); root_ = slot; configureRoot(); return true;
  }

  void draw(Canvas& canvas) const {
    Page* page = const_cast<Page*>(this);
    const Size display = canvas.size();
    page->draw(canvas, {0, 0, display.width, display.height});
  }

  void draw(Canvas& canvas, Rect contentArea) const {
    Page* page = const_cast<Page*>(this);
    if (!root_) { canvas.clear(background_); return; }
    page->viewport_ = contentArea;
    page->layout(contentArea);
    canvas.clear(background_);
    if (page->timed_) page->drawNavigation(canvas,contentArea);
    else { CanvasView view(canvas,contentArea,static_cast<int16_t>(-camera_.x),static_cast<int16_t>(-camera_.y)); root_->draw(view); }
  }

  void update(uint32_t nowMs) { nowMs_ = nowMs; timed_ = true; if (root_) root_->update(nowMs); }
  void setNavigationAnimation(uint16_t focusMs = kDefaultFocusDurationMs, uint16_t scrollMs = 280) {
    focusDurationMs_ = focusMs; scrollDurationMs_ = scrollMs;
    if (root_) root_->setScrollAnimation(scrollMs);
    if (!scrollMs) {
      cameraX_.start(cameraTarget_.x,cameraTarget_.x,nowMs_,0);
      cameraY_.start(cameraTarget_.y,cameraTarget_.y,nowMs_,0);
    }
  }
  Point scrollPosition() const { return camera_; }
  Rect focusVisualBounds() const {
    Rect result = focusBlob_.bounds(); result.x -= camera_.x; result.y -= camera_.y; return result;
  }
  bool focus(Component& target) {
    if (!target.visible() || !target.enabled()) return false;
    for (uint8_t i = 0; i < focusCount_; ++i) if (focusables_[i] == &target) {
      if (focusIndex_ != i) {
        focusables_[focusIndex_]->setFocused(false); focusIndex_ = i; target.setFocused(true);
      }
      verticalNavigation_ = false; navigationTarget_ = nullptr;
      followFocus_ = true;
      root_->ensureVisible(target); return true;
    }
    return false;
  }
  bool scrollAt(Point position, Point delta) {
    if (!laidOut_ || !containsPoint(viewport_,position)) return false;
    const Point world{static_cast<int16_t>(position.x+camera_.x),static_cast<int16_t>(position.y+camera_.y)};
    const bool consumedX = delta.x && root_->scrollAt(world,{delta.x,0});
    const bool consumedY = delta.y && root_->scrollAt(world,{0,delta.y});
    if (consumedX) delta.x = 0;
    if (consumedY) delta.y = 0;
    if (consumedX || consumedY) followFocus_ = false;
    const Rect extent = unionBounds(viewport_,root_->contentExtent());
    // Clamp the requested offset to content, without pulling it back to a control.
    const Point target{clampOffset(int32_t(cameraTarget_.x)+delta.x,extent.x-viewport_.x,extent.x+extent.width-viewport_.x-viewport_.width),
                       clampOffset(int32_t(cameraTarget_.y)+delta.y,extent.y-viewport_.y,extent.y+extent.height-viewport_.y-viewport_.height)};
    if (target.x == cameraTarget_.x && target.y == cameraTarget_.y) return consumedX || consumedY;
    cameraTarget_ = target; followFocus_ = false;
    cameraX_.retarget(target.x,nowMs_,timed_ ? scrollDurationMs_ : 0); cameraY_.retarget(target.y,nowMs_,timed_ ? scrollDurationMs_ : 0);
    if (!timed_) camera_ = target;
    return true;
  }
  void setBackground(Color color) { background_ = color; }
  Color background() const { return background_; }

  // Dispatches an already-normalized action. Input devices map their own
  // buttons/encoders to this enum (see InputMap below).
  bool dispatch(Action action) {
    if (!laidOut_) return false;
    if (action == Action::Use) return focusedComponent() && focusedComponent()->handle(action);
    if (action == Action::Back) return false;
    if (action == Action::Next) return focusCount_ && focusIndex_ + 1 < focusCount_ && focus(*focusables_[focusIndex_ + 1]);
    if (action == Action::Previous) return focusCount_ && focusIndex_ > 0 && focus(*focusables_[focusIndex_ - 1]);
    if (move(action)) return true;
    Point delta{0,0};
    if (action == Action::Down) delta.y = 8;
    else if (action == Action::Up) delta.y = -8;
    else if (action == Action::Right) delta.x = 8;
    else if (action == Action::Left) delta.x = -8;
    else return false;
    Point position{static_cast<int16_t>(viewport_.x+viewport_.width/2),static_cast<int16_t>(viewport_.y+viewport_.height/2)};
    if (focusedComponent()) {
      Rect current = focusedComponent()->bounds(); root_->locate(*focusedComponent(),current);
      const Point center{static_cast<int16_t>(current.x+current.width/2-camera_.x),static_cast<int16_t>(current.y+current.height/2-camera_.y)};
      if (containsPoint(viewport_,center)) position = center;
    }
    return scrollAt(position,delta);
  }

  Component* focusedComponent() const {
    return focusCount_ && focusIndex_ < focusCount_ ? focusables_[focusIndex_] : nullptr;
  }

  void resetFocus() {
    for (uint8_t i = 0; i < focusCount_; ++i) focusables_[i]->setFocused(false);
    focusIndex_ = 0;
    laidOut_ = false;
    focusCount_ = 0;
    lastVisualFocus_ = nullptr; focusBlob_.reset();
    verticalNavigation_ = false; navigationTarget_ = nullptr;
    cameraX_.start(0,0,nowMs_,0); cameraY_.start(0,0,nowMs_,0);
    camera_ = {0,0}; cameraTarget_ = {0,0};
  }

 private:
  void configureRoot() {
    if (!root_->hasWidthMode() && (!root_->hasFrame() || root_->frame().width.unit == Unit::Auto)) root_->setWidth(Length::percent(100));
    if (!root_->hasHeightMode() && (!root_->hasFrame() || root_->frame().height.unit == Unit::Auto)) root_->setHeight(Length::percent(100));
    root_->setScrollAnimation(scrollDurationMs_);
  }

  static int16_t clampOffset(int32_t value,int32_t low,int32_t high) {
    if (value < low) value = low;
    if (value > high) value = high;
    return value > 32767 ? 32767 : (value < -32768 ? -32768 : static_cast<int16_t>(value));
  }
  static int16_t cameraTarget(int16_t current, int32_t start, int32_t length,
                              int32_t viewStart, int32_t viewport, int32_t minimum, int32_t maximum) {
    const int32_t upper = start - viewStart;
    const int32_t lower = start + length - viewStart - viewport;
    int32_t next = current;
    if (length > viewport) next = start - viewStart;
    else { if (next < lower) next = lower; if (next > upper) next = upper; }
    if (next < minimum) next = minimum;
    if (next > maximum) next = maximum;
    return next > 32767 ? 32767 : (next < -32768 ? -32768 : static_cast<int16_t>(next));
  }
  static int16_t cameraPixel(float value) { return static_cast<int16_t>(value + (value < 0 ? -.5f : .5f)); }
  void drawNavigation(Canvas& canvas,Rect viewport) {
    Component* current = focusedComponent();
    const Rect extent = unionBounds(viewport,root_->contentExtent());
    const int32_t minX = extent.x, minY = extent.y;
    const int32_t maxX = int32_t(extent.x)+extent.width, maxY = int32_t(extent.y)+extent.height;
    if (current && followFocus_) {
      Rect target = current->bounds(); root_->locate(*current,target,true);
      Rect clip = unionBounds(extent,target); root_->focusClip(*current,clip); target = intersectBounds(target,clip);
      const Point next{
          cameraTarget(cameraTarget_.x,target.x,target.width,viewport.x,viewport.width,minX - viewport.x,maxX - viewport.x - viewport.width),
          cameraTarget(cameraTarget_.y,target.y,target.height,viewport.y,viewport.height,minY - viewport.y,maxY - viewport.y - viewport.height)};
      if (next.x != cameraTarget_.x) cameraX_.retarget(next.x,nowMs_,scrollDurationMs_);
      if (next.y != cameraTarget_.y) cameraY_.retarget(next.y,nowMs_,scrollDurationMs_);
      cameraTarget_ = next;
    }
    followFocus_ = false;
    const Point clamped{clampOffset(cameraTarget_.x,minX-viewport.x,maxX-viewport.x-viewport.width),
                        clampOffset(cameraTarget_.y,minY-viewport.y,maxY-viewport.y-viewport.height)};
    if (clamped.x != cameraTarget_.x) cameraX_.retarget(clamped.x,nowMs_,scrollDurationMs_);
    if (clamped.y != cameraTarget_.y) cameraY_.retarget(clamped.y,nowMs_,scrollDurationMs_);
    cameraTarget_ = clamped;
    camera_ = {cameraPixel(cameraX_.value(nowMs_)),cameraPixel(cameraY_.value(nowMs_))};
    if (current && focusDurationMs_) {
      Rect visual = current->bounds(); root_->locate(*current,visual);
      const Rect region = current->focusBounds();
      visual.x += region.x - current->bounds().x; visual.y += region.y - current->bounds().y;
      visual.width = region.width; visual.height = region.height;
      focusBlob_.follow(visual,current != lastVisualFocus_,nowMs_,focusDurationMs_,current->focusCornerCut());
      lastVisualFocus_ = current;
      current->setFocused(false);
      Rect clip = viewport; clip.x += camera_.x; clip.y += camera_.y;
      root_->focusClip(*current,clip); clip.x -= camera_.x; clip.y -= camera_.y;
      LiquidFocusCanvas liquid(canvas,focusBlob_,camera_,clip);
      liquid.fillRect(viewport,background_);
      CanvasView view(liquid,viewport,static_cast<int16_t>(-camera_.x),static_cast<int16_t>(-camera_.y));
      root_->draw(view);
      current->setFocused(true);
    } else {
      focusBlob_.reset(); lastVisualFocus_ = nullptr;
      CanvasView view(canvas,viewport,static_cast<int16_t>(-camera_.x),static_cast<int16_t>(-camera_.y));
      root_->draw(view);
    }
  }

  void layout(Rect screen) {
    Component* previous = focusedComponent();
    root_->layout(screen);
    focusCount_ = 0;
    root_->collectFocusables(focusables_, focusCount_, kMaxComponents);
    if (focusCount_ == 0) focusIndex_ = 0;
    else {
      focusIndex_ = 0;
      for (uint8_t i = 0; i < focusCount_; ++i) if (focusables_[i] == previous) focusIndex_ = i;
    }
    for (uint8_t i = 0; i < focusCount_; ++i) focusables_[i]->setFocused(i == focusIndex_);
    if (focusedComponent() != previous) { followFocus_ = true; if (focusCount_) root_->ensureVisible(*focusables_[focusIndex_]); }
    laidOut_ = true;
  }

  Rect navigationBounds(const Component& component, bool artwork = true) const {
    Rect located = component.bounds(); root_->locate(component,located,true);
    if (!artwork) return located;
    const Rect region = component.focusBounds();
    located.x += region.x-component.bounds().x; located.y += region.y-component.bounds().y;
    located.width = region.width; located.height = region.height;
    return located;
  }

  bool move(Action direction) {
    Component* current = focusedComponent();
    if (!current) return false;
    const bool vertical = direction == Action::Down || direction == Action::Up;
    if (!vertical && direction != Action::Left && direction != Action::Right) return false;
    const bool forward = direction == Action::Down || direction == Action::Right;
    const Rect origin = navigationBounds(*current,false);
    const Rect artwork = navigationBounds(*current);
    const int32_t crossCenter = vertical ? 2*artwork.x+artwork.width : 2*artwork.y+artwork.height;
    // Preserve the original column across rows with different button widths.
    const int32_t anchor = vertical && verticalNavigation_ && navigationTarget_ == current ? navigationAnchor_ : crossCenter;
    const auto eligible = [&](Rect candidate) {
      if (candidate.width <= 0 || candidate.height <= 0) return false;
      if (vertical) return forward ? candidate.y >= origin.y+origin.height : candidate.y+candidate.height <= origin.y;
      if (!overlap(candidate.y,candidate.y+candidate.height,origin.y,origin.y+origin.height)) return false;
      return forward ? candidate.x >= origin.x+origin.width : candidate.x+candidate.width <= origin.x;
    };
    const auto mainGap = [&](Rect candidate) {
      if (vertical) return forward ? candidate.y-(origin.y+origin.height) : origin.y-(candidate.y+candidate.height);
      return forward ? candidate.x-(origin.x+origin.width) : origin.x-(candidate.x+candidate.width);
    };
    // Choose the nearest row/column before comparing alignment. A later small
    // control must never win just because its center is closer than a tall one.
    Rect band{0,0,0,0};
    int32_t nearestGap = -1;
    for (uint8_t i = 0; i < focusCount_; ++i) {
      if (focusables_[i] == current) continue;
      const Rect candidate = navigationBounds(*focusables_[i],false);
      if (!eligible(candidate)) continue;
      const int32_t gap = mainGap(candidate);
      if (nearestGap < 0 || gap < nearestGap) { nearestGap = gap; band = candidate; }
    }
    if (nearestGap < 0) return false;
    int16_t best = -1;
    int32_t bestCross = 0, bestGap = 0;
    for (uint8_t i = 0; i < focusCount_; ++i) {
      if (focusables_[i] == current) continue;
      const Rect candidate = navigationBounds(*focusables_[i],false);
      if (!eligible(candidate)) continue;
      const bool sameBand = vertical ? overlap(candidate.y,candidate.y+candidate.height,band.y,band.y+band.height)
                                     : overlap(candidate.x,candidate.x+candidate.width,band.x,band.x+band.width);
      if (!sameBand) continue;
      const Rect candidateArtwork = navigationBounds(*focusables_[i]);
      const int32_t cross = distance(vertical ? 2*candidateArtwork.x+candidateArtwork.width : 2*candidateArtwork.y+candidateArtwork.height,anchor);
      const int32_t gap = mainGap(candidate);
      if (best < 0 || cross < bestCross || (cross == bestCross && gap < bestGap)) {
        best = i; bestCross = cross; bestGap = gap;
      }
    }
    if (best < 0 || !focus(*focusables_[best])) return false;
    verticalNavigation_ = vertical; navigationAnchor_ = anchor; navigationTarget_ = focusedComponent();
    return true;
  }

  static int32_t distance(int32_t a, int32_t b) { return a > b ? a - b : b - a; }
  static int16_t saturatingAdd(int16_t a, int16_t b) {
    const int32_t sum = static_cast<int32_t>(a) + b;
    return sum > 32767 ? 32767 : (sum < -32768 ? -32768 : static_cast<int16_t>(sum));
  }
  static bool overlap(int32_t aStart, int32_t aEnd, int32_t bStart, int32_t bEnd) {
    return aStart < bEnd && bStart < aEnd;
  }

  bool followFocus_ = true;
  Rect viewport_{0,0,0,0};
  bool verticalNavigation_ = false;
  int32_t navigationAnchor_ = 0;
  Component* navigationTarget_ = nullptr;
  Color background_;
  ComponentSlot root_;
  Component* focusables_[kMaxComponents]{};
  uint8_t focusCount_ = 0;
  uint8_t focusIndex_ = 0;
  bool laidOut_ = false;
  bool timed_ = false;
  uint32_t nowMs_ = 0;
  uint16_t focusDurationMs_ = kDefaultFocusDurationMs, scrollDurationMs_ = 280;
  Point camera_{0,0}, cameraTarget_{0,0};
  Tween cameraX_, cameraY_;
  LiquidFocus focusBlob_;
  Component* lastVisualFocus_ = nullptr;
};

inline void render(Canvas& display, const Page& page) {
  page.draw(display);
  display.present();
}

inline void render(Canvas& display, const Page& page, Rect contentArea) {
  page.draw(display, contentArea);
  display.present();
}

class Compositor {
 public:
  using PageId = uint8_t;
  static constexpr PageId kInvalidPage = 0xFF;
  static constexpr int16_t kUnlimitedFps = -1;
  static constexpr int16_t kDefaultFps = 30;
  // Fixed registry/history avoids heap use.
  static constexpr uint8_t kMaxPages = 16;
  // Fixed history avoids heap use.
  static constexpr uint8_t kMaxHistory = 8;

  PageId registerPage(Page& page) {
    const PageId existing = pageId(page);
    if (existing != kInvalidPage) return existing;
    if (pageCount_ == kMaxPages) return kInvalidPage;
    page.setNavigationAnimation(focusDurationMs_,scrollDurationMs_);
    pages_[pageCount_] = &page;
    return pageCount_++;
  }

  PageId pageId(const Page& page) const {
    for (uint8_t i = 0; i < pageCount_; ++i)
      if (pages_[i] == &page) return i;
    return kInvalidPage;
  }

  bool openPage(PageId id, bool remember = true) {
    if (id >= pageCount_) return false;
    if (current_ == id) return true;
    if (remember && current_ != kInvalidPage && historyCount_ == kMaxHistory) {
      for (uint8_t i = 1; i < kMaxHistory; ++i) history_[i - 1] = history_[i];
      --historyCount_;
    }
    if (remember && current_ != kInvalidPage) history_[historyCount_++] = current_;
    startTransition(false);
    current_ = id;
    pages_[current_]->resetFocus();
    return true;
  }

  // A page must be registered first. This catches accidental navigation to a
  // temporary/local page and keeps the registry the single source of truth.
  bool openPage(Page& page) {
    const PageId id = pageId(page);
    return id != kInvalidPage && openPage(id);
  }

  bool show(Page& page) { return page.valid() && openPage(registerPage(page)); }

  bool back() {
    if (historyCount_ == 0) return false;
    startTransition(true);
    current_ = history_[--historyCount_];
    pages_[current_]->resetFocus();
    return true;
  }

  bool dispatch(Action action) {
    if (current_ == kInvalidPage) return false;
    if (action == Action::Back) return back();
    return pages_[current_]->dispatch(action);
  }

  bool scrollAt(Point position, Point delta) {
    return current_ != kInvalidPage && pages_[current_]->scrollAt(position,delta);
  }

  void setSafeArea(Insets insets) { safeArea_ = insets; }
  Insets safeArea() const { return safeArea_; }

  // A positive value is the target display rate. -1 means render as fast as
  // the caller can request frames. Zero and other negative values are invalid
  // and fall back to the default 30 FPS.
  void setFps(int16_t fps) { fps_ = (fps == kUnlimitedFps || fps > 0) ? fps : kDefaultFps; }
  int16_t fps() const { return fps_; }
  uint16_t frameTimeMs() const {
    return fps_ == kUnlimitedFps ? 0 : static_cast<uint16_t>(1000 / fps_);
  }

  // Record the timestamp of a frame boundary. The caller supplies its native
  // monotonic clock (Arduino sketches can pass micros(); host tools can use a
  // steady clock). Unsigned subtraction keeps wraparound-safe intervals.
  void sampleFrame(uint32_t nowUs) const {
    if (hasFrameTimestamp_) {
      measuredFrameTimeUs_ = nowUs - lastFrameTimestampUs_;
      const uint32_t measured = measuredFrameTimeUs_ == 0 ? 0 : 1000000UL / measuredFrameTimeUs_;
      measuredFps_ = measured > 65535UL ? 65535U : static_cast<uint16_t>(measured);
    }
    lastFrameTimestampUs_ = nowUs;
    hasFrameTimestamp_ = true;
  }
  uint16_t actualFps() const { return measuredFps_; }
  uint16_t actualFrameTimeMs() const {
    const uint32_t milliseconds = measuredFrameTimeUs_ / 1000 + (measuredFrameTimeUs_ % 1000 >= 500 ? 1 : 0);
    return milliseconds > 65535 ? 65535 : static_cast<uint16_t>(milliseconds);
  }

  void render(Canvas& display) const {
    if (current_ != kInvalidPage) mad::render(display, *pages_[current_], contentArea(display.size()));
  }

  void setNavigationAnimation(uint16_t focusMs = kDefaultFocusDurationMs, uint16_t scrollMs = 280) {
    focusDurationMs_ = focusMs; scrollDurationMs_ = scrollMs;
    for (uint8_t i = 0; i < pageCount_; ++i) pages_[i]->setNavigationAnimation(focusMs,scrollMs);
  }

  void setTransition(uint16_t durationMs = 320, Easing easing = Easing::CubicInOut) {
    transitionDurationMs_ = durationMs;
    transitionEasing_ = easing;
  }

  void render(Canvas& display, uint32_t nowUs) const {
    const uint32_t deltaUs = hasFrameTimestamp_ ? nowUs - lastFrameTimestampUs_ : 0;
    sampleFrame(nowUs);
    // Accumulate deltas rather than nowUs/1000: micros() wraps every ~71 min.
    const uint64_t accumulated = static_cast<uint64_t>(animationRemainderUs_) + deltaUs;
    animationTimeMs_ += static_cast<uint32_t>(accumulated / 1000);
    animationRemainderUs_ = static_cast<uint16_t>(accumulated % 1000);
    if (current_ == kInvalidPage) return;
    pages_[current_]->update(animationTimeMs_);
    if (outgoing_ == kInvalidPage || transitionDurationMs_ == 0) {
      outgoing_ = kInvalidPage;
      render(display);
      return;
    }
    if (transitionPending_) {
      transition_.start(0, 1, animationTimeMs_, transitionDurationMs_, transitionEasing_);
      transitionPending_ = false;
    }
    const float progress = transition_.value(animationTimeMs_);
    if (!transition_.active(animationTimeMs_)) {
      outgoing_ = kInvalidPage;
      render(display);
      return;
    }
    const Size size = display.size();
    const Rect clip{0, 0, size.width, size.height};
    const int16_t travel = static_cast<int16_t>(size.width * progress + 0.5f);
    const int16_t direction = transitionBack_ ? -1 : 1;
    display.clear();
    CanvasView oldView(display, clip, static_cast<int16_t>(-direction * travel));
    CanvasView newView(display, clip, static_cast<int16_t>(direction * (size.width - travel)));
    pages_[outgoing_]->draw(oldView, contentArea(size));
    pages_[current_]->draw(newView, contentArea(size));
    display.present();
  }

  Page* currentPage() const { return current_ == kInvalidPage ? nullptr : pages_[current_]; }
  PageId currentPageId() const { return current_; }

 private:
  void startTransition(bool backwards) {
    outgoing_ = current_;
    transitionBack_ = backwards;
    transitionPending_ = true;
  }
  mutable Tween transition_;
  mutable PageId outgoing_ = kInvalidPage;
  mutable bool transitionPending_ = false;
  bool transitionBack_ = false;
  uint16_t transitionDurationMs_ = 0;
  Easing transitionEasing_ = Easing::CubicInOut;
  uint16_t focusDurationMs_ = kDefaultFocusDurationMs, scrollDurationMs_ = 280;
  mutable uint32_t animationTimeMs_ = 0;
  mutable uint16_t animationRemainderUs_ = 0;

  static int16_t clampInset(int16_t inset, int16_t available) {
    if (inset < 0) return 0;
    return inset > available ? available : inset;
  }

  Rect contentArea(Size display) const {
    const int16_t left = clampInset(safeArea_.left, display.width);
    const int16_t top = clampInset(safeArea_.top, display.height);
    const int16_t right = clampInset(safeArea_.right, static_cast<int16_t>(display.width - left));
    const int16_t bottom = clampInset(safeArea_.bottom, static_cast<int16_t>(display.height - top));
    return {left, top,
            static_cast<int16_t>(display.width - left - right),
            static_cast<int16_t>(display.height - top - bottom)};
  }

  Page* pages_[kMaxPages]{};
  PageId history_[kMaxHistory]{};
  PageId current_ = kInvalidPage;
  uint8_t pageCount_ = 0;
  uint8_t historyCount_ = 0;
  Insets safeArea_{1, 1, 1, 1};
  int16_t fps_ = kDefaultFps;
  mutable uint32_t lastFrameTimestampUs_ = 0;
  mutable uint32_t measuredFrameTimeUs_ = 0;
  mutable uint16_t measuredFps_ = 0;
  mutable bool hasFrameTimestamp_ = false;
};

// The framework receives normalized actions. InputMap lets a sketch bind any
// board-specific key/encoder code to those actions without touching the UI.
class InputMap {
 public:
  // Fixed binding capacity keeps input handling heap-free.
  static constexpr uint8_t kMaxBindings = 16;

  bool bind(uint16_t code, Action action) {
    for (uint8_t i = 0; i < count_; ++i) {
      if (bindings_[i].code == code) {
        bindings_[i].action = action;
        return true;
      }
    }
    if (count_ == kMaxBindings) return false;
    bindings_[count_++] = {code, action};
    return true;
  }

  bool dispatch(uint16_t code, Compositor& compositor) const {
    for (uint8_t i = 0; i < count_; ++i)
      if (bindings_[i].code == code) return compositor.dispatch(bindings_[i].action);
    return false;
  }

  enum class Event : uint8_t { Pressed, Released, Repeat, LongPress };
  using EventHandler = void (*)(uint16_t code, Action action, Event event, void* context);

  void setDebounceMs(uint16_t milliseconds) { debounceMs_ = milliseconds; }
  uint16_t debounceMs() const { return debounceMs_; }
  void setRepeat(uint16_t delayMs, uint16_t intervalMs) {
    repeatDelayMs_ = delayMs;
    repeatIntervalMs_ = intervalMs;
  }
  void setLongPressMs(uint16_t milliseconds) { longPressMs_ = milliseconds; }
  uint16_t longPressMs() const { return longPressMs_; }
  void onEvent(EventHandler handler, void* context = nullptr) {
    eventHandler_ = handler;
    eventContext_ = context;
  }
  void reset() {
    for (uint8_t i = 0; i < count_; ++i) states_[i] = InputState{};
  }

  // Call once per loop with the current electrical state of a bound input.
  // The first press dispatches immediately; later transitions are debounced.
  bool update(uint16_t code, bool pressed, uint32_t nowMs, Compositor& compositor) {
    const int8_t index = bindingIndex(code);
    if (index < 0) return false;
    const Binding& binding = bindings_[index];
    InputState& state = states_[index];
    if (!state.initialized) {
      state.initialized = true;
      state.stablePressed = pressed;
      if (!pressed) return false;
      state.pressedAt = nowMs;
      state.nextRepeat = nowMs + repeatDelayMs_;
      compositor.dispatch(binding.action);
      emit(code, binding.action, Event::Pressed);
      return true;
    }

    if (pressed != state.stablePressed) {
      if (!state.pending || state.pendingPressed != pressed) {
        state.pending = true;
        state.pendingPressed = pressed;
        state.pendingAt = nowMs;
      }
      if (!elapsed(nowMs, state.pendingAt, debounceMs_)) return false;
      state.pending = false;
      state.stablePressed = pressed;
      if (pressed) {
        state.pressedAt = nowMs;
        state.nextRepeat = nowMs + repeatDelayMs_;
        state.longSent = false;
        compositor.dispatch(binding.action);
        emit(code, binding.action, Event::Pressed);
      } else {
        emit(code, binding.action, Event::Released);
      }
      return true;
    }
    state.pending = false;
    if (!state.stablePressed) return false;

    bool handled = false;
    if (longPressMs_ && !state.longSent && elapsed(nowMs, state.pressedAt, longPressMs_)) {
      state.longSent = true;
      emit(code, binding.action, Event::LongPress);
      handled = true;
    }
    if (repeatIntervalMs_ && reached(nowMs, state.nextRepeat)) {
      state.nextRepeat = nowMs + repeatIntervalMs_;
      compositor.dispatch(binding.action);
      emit(code, binding.action, Event::Repeat);
      handled = true;
    }
    return handled;
  }

 private:
  struct Binding { uint16_t code; Action action; };
  struct InputState {
    bool initialized = false;
    bool stablePressed = false;
    bool pending = false;
    bool pendingPressed = false;
    bool longSent = false;
    uint32_t pendingAt = 0;
    uint32_t pressedAt = 0;
    uint32_t nextRepeat = 0;
  };

  int8_t bindingIndex(uint16_t code) const {
    for (uint8_t i = 0; i < count_; ++i)
      if (bindings_[i].code == code) return static_cast<int8_t>(i);
    return -1;
  }

  void emit(uint16_t code, Action action, Event event) const {
    if (eventHandler_) eventHandler_(code, action, event, eventContext_);
  }

  static bool elapsed(uint32_t now, uint32_t since, uint16_t milliseconds) {
    return static_cast<uint32_t>(now - since) >= milliseconds;
  }

  static bool reached(uint32_t now, uint32_t deadline) {
    return static_cast<int32_t>(now - deadline) >= 0;
  }

  Binding bindings_[kMaxBindings]{};
  InputState states_[kMaxBindings]{};
  uint8_t count_ = 0;
  uint16_t debounceMs_ = 30;
  uint16_t repeatDelayMs_ = 500;
  uint16_t repeatIntervalMs_ = 0;
  uint16_t longPressMs_ = 0;
  EventHandler eventHandler_ = nullptr;
  void* eventContext_ = nullptr;
};

}  // namespace mad
