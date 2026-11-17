#pragma once
#include "matrix.hpp"
#include "content.hpp"

namespace mad {

inline void drawLine(Canvas& canvas, Point a, Point b, Color color) {
  int32_t x = a.x, y = a.y;
  const int32_t dx = a.x < b.x ? b.x-a.x : a.x-b.x;
  const int32_t dy = a.y < b.y ? a.y-b.y : b.y-a.y;
  const int16_t sx = a.x < b.x ? 1 : -1, sy = a.y < b.y ? 1 : -1;
  int32_t error = dx + dy;
  for (;;) {
    canvas.pixel(static_cast<int16_t>(x),static_cast<int16_t>(y),color);
    if (x == b.x && y == b.y) break;
    const int32_t twice = 2 * error;
    if (twice >= dy) { error += dy; x += sx; }
    if (twice <= dx) { error += dx; y += sy; }
  }
}

inline void drawEllipse(Canvas& canvas, Rect rect, Color color, bool filled = false) {
  if (rect.width <= 0 || rect.height <= 0) return;
  const int64_t ww = int64_t(rect.width)*rect.width, hh = int64_t(rect.height)*rect.height;
  const auto leftEdge = [&](int32_t y) {
    if (y < 0 || y >= rect.height) return int32_t(rect.width);
    const int64_t dy = 2*y-rect.height+1;
    int32_t low = 0, high = (rect.width-1)/2;
    while (low < high) {
      const int32_t middle = (low+high)/2;
      const int64_t dx = 2*middle-rect.width+1;
      if (dx*dx*hh+dy*dy*ww <= ww*hh) high = middle; else low = middle+1;
    }
    const int64_t dx = 2*low-rect.width+1;
    return dx*dx*hh+dy*dy*ww <= ww*hh ? low : int32_t(rect.width);
  };
  const Rect clip = canvas.viewport();
  const int32_t top = rect.y < clip.y ? clip.y-rect.y : 0;
  const int32_t end = int32_t(rect.y)+rect.height > int32_t(clip.y)+clip.height ? int32_t(clip.y)+clip.height-rect.y : rect.height;
  for (int32_t y = top; y < end; ++y) {
    const int32_t left = leftEdge(y), right = rect.width-1-left;
    if (left > right) continue;
    if (filled) canvas.fillSpan(rect.x+left,rect.y+y,right-left+1,color);
    else {
      const int32_t before = leftEdge(y-1), after = leftEdge(y+1);
      int32_t leftEnd = left+1, rightStart = right;
      if (before > leftEnd) leftEnd = before;
      if (after > leftEnd) leftEnd = after;
      if (rect.width-before < rightStart) rightStart = rect.width-before;
      if (rect.width-after < rightStart) rightStart = rect.width-after;
      if (leftEnd >= rightStart) canvas.fillSpan(rect.x+left,rect.y+y,right-left+1,color);
      else {
        canvas.fillSpan(rect.x+left,rect.y+y,leftEnd-left,color);
        canvas.fillSpan(rect.x+rightStart,rect.y+y,right-rightStart+1,color);
      }
    }
  }
}

enum class DrawingKind : uint8_t { Pixel, Line, Rectangle, Ellipse };
struct DrawCommand {
  DrawingKind kind = DrawingKind::Line;
  Rect rect{0,0,24,16};
  Color color = Color::On;
  bool filled = false;
  void draw(Canvas& canvas) const {
    switch (kind) {
      case DrawingKind::Pixel: canvas.pixel(rect.x,rect.y,color); break;
      case DrawingKind::Line: drawLine(canvas,{rect.x,rect.y},{static_cast<int16_t>(rect.x+rect.width-1),static_cast<int16_t>(rect.y+rect.height-1)},color); break;
      case DrawingKind::Rectangle: if (filled) canvas.fillRect(rect,color); else canvas.strokeRect(rect,color); break;
      case DrawingKind::Ellipse: drawEllipse(canvas,rect,color,filled); break;
    }
  }
};

// Custom drawing uses local coordinates and is clipped to the component bounds.
class CanvasComponent final : public Component {
 public:
  MAD_DETAIL_COMPONENT_CHAIN(CanvasComponent)
  using DrawHandler = void (*)(Canvas&, Size, uint32_t nowMs, void* context);
  CanvasComponent(Size size, DrawHandler draw, void* context = nullptr)
      : size_(size), draw_(draw), context_(context) {}
  void setPreferredSize(Size size) { size_ = size; }
  void setDrawHandler(DrawHandler draw, void* context = nullptr) { draw_ = draw; context_ = context; }
  Size preferredSize() const override { return size_; }
  void update(uint32_t nowMs) override { nowMs_ = nowMs; }
  void draw(Canvas& canvas) const override {
    if (!visible_ || !draw_ || bounds_.width <= 0 || bounds_.height <= 0) return;
    ContentCanvas local(canvas,bounds_);
    draw_(local,{bounds_.width,bounds_.height},nowMs_,context_);
  }
 private:
  Size size_;
  DrawHandler draw_;
  void* context_;
  uint32_t nowMs_ = 0;
};

// Row-major pixels, nearest-neighbor scaling, no owned buffer or heap allocation.
// Keep the source pixels alive while this component is in use.
class ImageComponent final : public Component {
 public:
  MAD_DETAIL_COMPONENT_CHAIN(ImageComponent)
  ImageComponent(const MatrixPixel* pixels, Size size, Color black = Color::Off, Color white = Color::On)
      : pixels_(pixels), size_(size), black_(black), white_(white) {}
  // Coverage: 0 transparent, 1 normal, 2 fixed (bypasses focus inversion).
  ImageComponent(const Color* pixels,Size size,const uint8_t* coverage = nullptr)
      : pixels_(nullptr), size_(size), black_(Color::Off), white_(Color::On), colors_(pixels), coverage_(coverage) {}
  Size preferredSize() const override { return size_; }
  void draw(Canvas& canvas) const override {
    if (!visible_ || (!pixels_ && !colors_) || size_.width <= 0 || size_.height <= 0 || bounds_.width <= 0 || bounds_.height <= 0) return;
    const Rect clip = canvas.viewport();
    const int32_t top = bounds_.y < clip.y ? clip.y-bounds_.y : 0;
    const int32_t left = bounds_.x < clip.x ? clip.x-bounds_.x : 0;
    const int32_t bottom = int32_t(bounds_.y)+bounds_.height > int32_t(clip.y)+clip.height ? int32_t(clip.y)+clip.height-bounds_.y : bounds_.height;
    const int32_t right = int32_t(bounds_.x)+bounds_.width > int32_t(clip.x)+clip.width ? int32_t(clip.x)+clip.width-bounds_.x : bounds_.width;
    for (int32_t y = top; y < bottom; ++y) for (int32_t x = left; x < right; ++x) {
      const size_t index = (y*size_.height/bounds_.height)*size_.width+x*size_.width/bounds_.width;
      if (colors_) {
        const uint8_t coverage = coverage_ ? coverage_[index] : 1;
        if (coverage > 1) canvas.fixedPixel(bounds_.x+x,bounds_.y+y,colors_[index]);
        else if (coverage) canvas.pixel(bounds_.x+x,bounds_.y+y,colors_[index]);
      } else {
        const MatrixPixel pixel = pixels_[index];
        if (pixel == BF || pixel == WF) canvas.fixedPixel(bounds_.x+x,bounds_.y+y,matrixColor(pixel,false,black_,white_));
        else if (pixel != N) canvas.pixel(bounds_.x+x,bounds_.y+y,matrixColor(pixel,false,black_,white_));
      }
    }
  }

 private:
  const MatrixPixel* pixels_;
  Size size_;
  Color black_, white_;
  const Color* colors_ = nullptr;
  const uint8_t* coverage_ = nullptr;
};

class ShapeComponent final : public Component {
 public:
  MAD_DETAIL_COMPONENT_CHAIN(ShapeComponent)
  ShapeComponent(DrawingKind kind, Size size = {24,16}, Color color = Color::On, bool filled = false)
      : size_(size) { command_.kind = kind; command_.color = color; command_.filled = filled; }
  Size preferredSize() const override { return size_; }
  void draw(Canvas& canvas) const override {
    if (!visible_ || bounds_.width <= 0 || bounds_.height <= 0) return;
    DrawCommand command = command_; command.rect = bounds_; command.draw(canvas);
  }
 private:
  Size size_;
  DrawCommand command_;
};

}  // namespace mad
