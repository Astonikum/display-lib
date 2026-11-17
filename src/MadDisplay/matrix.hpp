#pragma once

#include "core.hpp"

namespace mad {

// Pixel artwork for fixed-size components. N leaves the canvas untouched;
// BF/WF keep their color when focused; B/W swap their colors when focused.
enum MatrixPixel : uint8_t { N, BF, WF, B, W };

inline Color matrixColor(MatrixPixel pixel, bool focused,
                         Color black = Color::Off, Color white = Color::On) {
  switch (pixel) {
    case BF: return black;
    case WF: return white;
    case B: return focused ? inverted(black) : black;
    case W: return focused ? inverted(white) : white;
    case N: return Color::Off;
  }
  return Color::Off;
}

template <uint8_t Height, uint8_t Width>
inline void drawMatrix(Canvas& canvas, int16_t x, int16_t y,
                       const MatrixPixel (&matrix)[Height][Width], bool focused,
                       Color black = Color::Off, Color white = Color::On) {
  for (uint8_t row = 0; row < Height; ++row)
    for (uint8_t column = 0; column < Width; ++column) {
      const MatrixPixel pixel = matrix[row][column];
      if (pixel == BF || pixel == WF)
        canvas.fixedPixel(static_cast<int16_t>(x + column),static_cast<int16_t>(y + row),matrixColor(pixel,focused,black,white));
      else if (pixel != N)
        canvas.pixel(static_cast<int16_t>(x + column),static_cast<int16_t>(y + row),matrixColor(pixel,focused,black,white));
    }
}

// A fixed-size, zero-allocation pixel-art component. N pixels are transparent;
// B/W pixels swap black and white while focused, BF/WF pixels do not.
template <uint8_t Height, uint8_t Width>
class MatrixComponent final : public Component {
 public:
  MatrixComponent(const MatrixPixel (&matrix)[Height][Width],
                  Color black = Color::Off, Color white = Color::On)
      : Component(), matrix_(matrix), black_(black), white_(white), focused_(false) {}
  MatrixComponent(SizeSpec size, const MatrixPixel (&matrix)[Height][Width],
                  Color black = Color::Off, Color white = Color::On)
      : Component(size), matrix_(matrix), black_(black), white_(white), focused_(false) {}
  MatrixComponent(Frame frame, const MatrixPixel (&matrix)[Height][Width],
                  Color black = Color::Off, Color white = Color::On)
      : Component(frame), matrix_(matrix), black_(black), white_(white), focused_(false) {}

  Size preferredSize() const override { return {Width, Height}; }
  bool focused() const override { return focused_; }
  void setFocused(bool focused) override { focused_ = focused; }
  void setColors(Color black, Color white) { black_ = black; white_ = white; }

  void draw(Canvas& canvas) const override {
    if (!visible_ || bounds_.width <= 0 || bounds_.height <= 0) return;
    const int16_t x = static_cast<int16_t>(bounds_.x + (bounds_.width - Width) / 2);
    const int16_t y = static_cast<int16_t>(bounds_.y + (bounds_.height - Height) / 2);
    drawMatrix(canvas, x, y, matrix_, focused_, black_, white_);
  }

 private:
  const MatrixPixel (&matrix_)[Height][Width];
  Color black_;
  Color white_;
  bool focused_;
};

}  // namespace mad
