#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "core.hpp"

namespace mad {

// A readable Canvas target. It stores rendered pixels but never talks to a
// physical device; host tools can inspect the packed framebuffer directly.
class Buffer : public Canvas {
 public:
  using Canvas::pixel;
  virtual int16_t width() const = 0;
  virtual int16_t height() const = 0;
  virtual bool isOn(int16_t x, int16_t y) const = 0;
  virtual Color pixel(int16_t x, int16_t y) const = 0;
  virtual const uint8_t* pixels() const = 0;
  virtual uint8_t* pixels() = 0;
  virtual size_t byteCount() const = 0;
};

// The packed byte layout is part of the public contract: pixels are stored
// row-major, with pixel (x, y) at bit (y * Width + x). Device adapters can
// translate this canonical layout to whatever their controller expects.
template <int16_t Width = 128, int16_t Height = 64>
class BareBuffer : public Buffer {
 public:
  static_assert(Width > 0 && Height > 0, "Buffer dimensions must be positive");
  using Canvas::text;
  static constexpr size_t kByteCount = (uint32_t(Width)*Height+7)/8;
  static_assert((uint32_t(Width)*Height+7)/8 <= SIZE_MAX,"Packed buffer exceeds addressable storage");

  BareBuffer() : bytes_{} {}

  Size size() const override { return {Width, Height}; }
  PixelFormat format() const override { return PixelFormat::Mono1; }
  int16_t width() const override { return Width; }
  int16_t height() const override { return Height; }

  void clear(Color color = Color::Off) override {
    for (size_t i = 0; i < kByteCount; ++i) bytes_[i] = monochromeOn(color) ? 0xFF : 0;
  }

  void pixel(int16_t x, int16_t y, Color color) override { setPixel(x, y, color); }

  Color pixel(int16_t x, int16_t y) const override { return isOn(x, y) ? Color::On : Color::Off; }

  void fillRect(Rect rect, Color color) override {
    const int32_t rightEdge = static_cast<int32_t>(rect.x) + rect.width;
    const int32_t bottomEdge = static_cast<int32_t>(rect.y) + rect.height;
    const int16_t left = rect.x < 0 ? 0 : (rect.x > Width ? Width : rect.x);
    const int16_t top = rect.y < 0 ? 0 : (rect.y > Height ? Height : rect.y);
    const int16_t right = rightEdge < 0 ? 0 : (rightEdge > Width ? Width : static_cast<int16_t>(rightEdge));
    const int16_t bottom = bottomEdge < 0 ? 0 : (bottomEdge > Height ? Height : static_cast<int16_t>(bottomEdge));
    if (left >= right || top >= bottom) return;

    // Fill contiguous bit spans a byte at a time.
    for (int16_t y = top; y < bottom; ++y) {
      uint32_t bit = uint32_t(y)*Width+left;
      size_t remaining = static_cast<size_t>(right - left);
      size_t byteIndex = bit >> 3;
      uint8_t shift = static_cast<uint8_t>(bit & 7u);
      if (shift != 0) {
        const uint8_t count = remaining < static_cast<size_t>(8 - shift)
            ? static_cast<uint8_t>(remaining) : static_cast<uint8_t>(8 - shift);
        const uint8_t mask = static_cast<uint8_t>(((1u << count) - 1u) << shift);
        applyMask(bytes_[byteIndex], mask, color);
        remaining -= count;
        ++byteIndex;
      }
      while (remaining >= 8) {
        bytes_[byteIndex++] = monochromeOn(color) ? 0xFF : 0;
        remaining -= 8;
      }
      if (remaining != 0) {
        const uint8_t mask = static_cast<uint8_t>((1u << remaining) - 1u);
        applyMask(bytes_[byteIndex], mask, color);
      }
    }
  }

  void fillSpan(int16_t x,int16_t y,int16_t length,Color color) override { fillRect({x,y,length,1},color); }

  void strokeRect(Rect rect, Color color) override {
    if (rect.width <= 0 || rect.height <= 0) return;
    fillRect({rect.x, rect.y, rect.width, 1}, color);
    fillRect({rect.x, static_cast<int16_t>(rect.y + rect.height - 1), rect.width, 1}, color);
    fillRect({rect.x, rect.y, 1, rect.height}, color);
    fillRect({static_cast<int16_t>(rect.x + rect.width - 1), rect.y, 1, rect.height}, color);
  }

  void text(int16_t x, int16_t y, const char* value, uint8_t scale, Color color) override {
    if (!value || scale == 0) return;
    drawDefaultText(*this, x, y, value, scale, color);
  }

  // A buffer has no hardware flush step.
  void present() override {}

  bool isOn(int16_t x, int16_t y) const override {
    if (x < 0 || y < 0 || x >= Width || y >= Height) return false;
    const uint32_t index = uint32_t(y)*Width+x;
    return (bytes_[index >> 3] & (1u << (index & 7u))) != 0;
  }

  const uint8_t* pixels() const override { return bytes_; }
  uint8_t* pixels() override { return bytes_; }
  size_t byteCount() const override { return kByteCount; }

 private:
  void setPixel(int16_t x, int16_t y, Color color) {
    if (x < 0 || y < 0 || x >= Width || y >= Height) return;
    const uint32_t index = uint32_t(y)*Width+x;
    applyMask(bytes_[index >> 3], static_cast<uint8_t>(1u << (index & 7u)), color);
  }

  static void applyMask(uint8_t& byte, uint8_t mask, Color color) {
    if (monochromeOn(color)) byte |= mask;
    else byte &= static_cast<uint8_t>(~mask);
  }

  uint8_t bytes_[kByteCount];
};

// Non-owning RGB565 storage, suitable for externally supplied RAM/PSRAM.
class RGB565Surface : public Buffer {
 public:
  using Canvas::text;
  bool setStorage(Size dimensions,uint16_t* storage,size_t capacity) {
    if (!storage || dimensions.width <= 0 || dimensions.height <= 0 ||
        uint32_t(dimensions.width)*dimensions.height > capacity ||
        uint64_t(dimensions.width)*dimensions.height > SIZE_MAX/sizeof(uint16_t)) return false;
    size_ = dimensions; storage_ = storage; return true;
  }
  Size size() const override { return size_; }
  PixelFormat format() const override { return PixelFormat::RGB565; }
  int16_t width() const override { return size_.width; }
  int16_t height() const override { return size_.height; }
  Color pixel(int16_t x,int16_t y) const override {
    return storage_ && x >= 0 && y >= 0 && x < width() && y < height() ? fromRGB565(storage_[size_t(y)*width()+x]) : Color::Off;
  }
  bool isOn(int16_t x,int16_t y) const override { return pixel(x,y) != Color::Black; }
  void pixel(int16_t x,int16_t y,Color color) override {
    if (storage_ && x >= 0 && y >= 0 && x < width() && y < height()) storage_[size_t(y)*width()+x] = rgb565(color);
  }
  void fillSpan(int16_t x,int16_t y,int16_t length,Color color) override {
    if (!storage_ || y < 0 || y >= height() || length <= 0) return;
    const int32_t right = int32_t(x)+length < width() ? int32_t(x)+length : width();
    const int32_t left = x < 0 ? 0 : x;
    if (left < right) {
      uint16_t* pixel = storage_+size_t(y)*width()+left;
      uint16_t* end = storage_+size_t(y)*width()+right;
      const uint16_t value = rgb565(color); while (pixel < end) *pixel++ = value;
    }
  }
  void clear(Color color = Color::Off) override {
    if (!storage_) return;
    uint16_t* pixel = storage_; uint16_t* end = storage_+size_t(width())*height();
    const uint16_t value = rgb565(color); while (pixel < end) *pixel++ = value;
  }
  void fillRect(Rect rect,Color color) override {
    const int32_t bottom = int32_t(rect.y)+rect.height < height() ? int32_t(rect.y)+rect.height : height();
    for (int32_t y = rect.y < 0 ? 0 : rect.y; y < bottom; ++y) fillSpan(rect.x,static_cast<int16_t>(y),rect.width,color);
  }
  void text(int16_t x,int16_t y,const char* value,uint8_t scale,Color color) override { drawDefaultText(*this,x,y,value,scale,color); }
  void present() override {}
  const uint8_t* pixels() const override { return reinterpret_cast<const uint8_t*>(storage_); }
  uint8_t* pixels() override { return reinterpret_cast<uint8_t*>(storage_); }
  const uint16_t* rgbPixels() const { return storage_; }
  uint16_t* rgbPixels() { return storage_; }
  size_t byteCount() const override { return size_t(width())*height()*sizeof(uint16_t); }
 private:
  Size size_{0,0};
  uint16_t* storage_ = nullptr;
};

template<int16_t Width,int16_t Height>
class RGB565Buffer : public RGB565Surface {
 public:
  static_assert(Width > 0 && Height > 0,"Buffer dimensions must be positive");
  static constexpr size_t kPixelCount = uint32_t(Width)*Height;
  static_assert(uint32_t(Width)*Height <= SIZE_MAX/sizeof(uint16_t),"RGB buffer exceeds addressable storage");
  RGB565Buffer() : storage_{} { setStorage({Width,Height},storage_,kPixelCount); }
  RGB565Buffer(const RGB565Buffer&) = delete;
  RGB565Buffer& operator=(const RGB565Buffer&) = delete;
 private:
  uint16_t storage_[kPixelCount];
};

// Packed palette indices: one bit for two colors, two bits for up to four.
template<int16_t Width,int16_t Height,uint8_t Bits = 2>
class PaletteBuffer : public Buffer {
 public:
  static_assert(Width > 0 && Height > 0,"Buffer dimensions must be positive");
  static constexpr size_t kByteCount = (uint32_t(Width)*Height*Bits+7)/8;
  static_assert((uint32_t(Width)*Height*Bits+7)/8 <= SIZE_MAX,"Palette buffer exceeds addressable storage");
  static_assert(Bits == 1 || Bits == 2 || Bits == 4,"Palette buffers support one, two or four bits per pixel");
  static constexpr uint8_t kPerByte = 8/Bits;
  static constexpr uint8_t kMask = (1u<<Bits)-1;
  explicit PaletteBuffer(Palette palette = {}) : palette_(palette), bytes_{} { if (palette_.count > (1u<<Bits)) palette_.count = (1u<<Bits); }
  Size size() const override { return {Width,Height}; }
  PixelFormat format() const override { return Bits == 1 ? PixelFormat::Indexed1 : Bits == 2 ? PixelFormat::Indexed2 : PixelFormat::Indexed4; }
  int16_t width() const override { return Width; }
  int16_t height() const override { return Height; }
  const Palette& palette() const { return palette_; }
  void setPalette(Palette palette) { palette_ = palette; if (palette_.count > (1u<<Bits)) palette_.count = (1u<<Bits); cached_ = false; }
  Color pixel(int16_t x,int16_t y) const override {
    if (x < 0 || y < 0 || x >= Width || y >= Height) return Color::Off;
    const uint32_t index = uint32_t(y)*Width+x;
    return palette_.colors[(bytes_[index/kPerByte] >> ((index%kPerByte)*Bits))&kMask];
  }
  bool isOn(int16_t x,int16_t y) const override { return pixel(x,y) != Color::Black; }
  void pixel(int16_t x,int16_t y,Color color) override {
    if (x < 0 || y < 0 || x >= Width || y >= Height) return;
    const uint32_t index = uint32_t(y)*Width+x; const uint8_t shift = (index%kPerByte)*Bits;
    bytes_[index/kPerByte] = (bytes_[index/kPerByte]&~(uint32_t(kMask)<<shift)) | (mapped(color)<<shift);
  }
  void clear(Color color = Color::Off) override { memset(bytes_,mapped(color)*(Bits == 1 ? 0xffu : Bits == 2 ? 0x55u : 0x11u),kByteCount); }
  void fillSpan(int16_t x,int16_t y,int16_t length,Color color) override {
    if (y < 0 || y >= Height || length <= 0) return;
    const int32_t right = int32_t(x)+length < Width ? int32_t(x)+length : Width;
    int32_t left = x < 0 ? 0 : x;
    if (left >= right) return;
    const uint8_t value = mapped(color);
    uint32_t bit = uint32_t(y)*Width+left, end = uint32_t(y)*Width+right;
    while (bit < end && bit%kPerByte) { setIndex(bit++,value); }
    const size_t bytes = (end-bit)/kPerByte;
    if (bytes) { memset(bytes_+bit/kPerByte,value*(Bits == 1 ? 0xffu : Bits == 2 ? 0x55u : 0x11u),bytes); bit += bytes*kPerByte; }
    while (bit < end) setIndex(bit++,value);
  }
  void text(int16_t x,int16_t y,const char* value,uint8_t scale,Color color) override { drawDefaultText(*this,x,y,value,scale,color); }
  void present() override {}
  const uint8_t* pixels() const override { return bytes_; }
  uint8_t* pixels() override { return bytes_; }
  size_t byteCount() const override { return kByteCount; }
 private:
  uint8_t mapped(Color color) {
    if (!cached_ || color != cachedColor_) { cachedColor_ = color; cachedIndex_ = palette_.index(color); cached_ = true; }
    return cachedIndex_;
  }
  void setIndex(uint32_t index,uint8_t value) {
    const uint8_t shift = (index%kPerByte)*Bits;
    bytes_[index/kPerByte] = (bytes_[index/kPerByte]&~(uint32_t(kMask)<<shift)) | (value<<shift);
  }
  Palette palette_;
  uint8_t bytes_[kByteCount];
  Color cachedColor_ = Color::Off;
  uint8_t cachedIndex_ = 0;
  bool cached_ = false;
};

template<int16_t Width,int16_t Height>
using TwoColorBuffer = PaletteBuffer<Width,Height,1>;

}  // namespace mad
