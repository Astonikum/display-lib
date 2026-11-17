#pragma once

#include <MadDisplay/MadDisplay.hpp>
#include <math.h>

namespace mad_demo {

// Same animated scene on a hardware Canvas and in the native simulator.
class Visualization final : public mad::Component {
 public:
  explicit Visualization(uint8_t mode) : mode_(mode) {
    setSizeMode(mad::SizeMode::Max, mad::SizeMode::Max);
  }
  mad::Size preferredSize() const override { return {126, 36}; }
  void update(uint32_t nowMs) override {
    if (hasTimestamp_) phase_ += static_cast<uint32_t>(nowMs - lastMs_) * 0.001f;
    hasTimestamp_ = true; lastMs_ = nowMs;
    // All scene frequencies are multiples of 0.01: wrap at their shared period.
    phase_ = fmodf(phase_, 628.318531f);
  }
  void draw(mad::Canvas& canvas) const override {
    if (!visible_ || bounds_.width < 1 || bounds_.height < 1) return;
    mad::CanvasView view(canvas, bounds_);
    const int w = bounds_.width, h = bounds_.height;
    if (mode_ == 0) {
      for (int wave = 0; wave < 3; ++wave) {
        int previous = h / 2;
        for (int x = 0; x < w; ++x) {
          const float amplitude = (0.22f + 0.06f * sinf(phase_ * 1.3f)) * h;
          const int y = static_cast<int>(h / 2 + amplitude * sinf(x * 0.065f + phase_ * 2.4f + wave * 1.3f)
              * cosf(x * 0.018f - phase_ * 0.7f));
          if (x) line(view, bounds_.x + x - 1, bounds_.y + previous, bounds_.x + x, bounds_.y + y);
          previous = y;
        }
      }
    } else if (mode_ == 1) {
      const float radius = (h < w ? h : w) * 0.40f;
      for (int orbit = 0; orbit < 3; ++orbit)
        for (int dot = 0; dot < 48; ++dot) {
          const float a = dot * 6.2831853f / 48 + phase_ * (0.6f + orbit * 0.2f);
          const float tilt = phase_ * 0.35f + orbit * 1.0472f;
          const float px = cosf(a) * radius, py = sinf(a) * radius * 0.35f;
          view.pixel(bounds_.x + w / 2 + static_cast<int>(px * cosf(tilt) - py * sinf(tilt)),
                     bounds_.y + h / 2 + static_cast<int>(px * sinf(tilt) + py * cosf(tilt)), mad::Color::On);
        }
      view.fillRect({static_cast<int16_t>(bounds_.x + w / 2 - 1),
                     static_cast<int16_t>(bounds_.y + h / 2 - 1), 3, 3}, mad::Color::On);
    } else {
      const int bars = w < 16 ? w : 16;
      for (int i = 0; i < bars; ++i) {
        const float pulse = 0.5f + 0.5f * sinf(phase_ * (2.1f + i * 0.07f) + i * 0.7f);
        const int height = static_cast<int>(2 + pulse * pulse * (h - 3));
        const int left = i * w / bars;
        const int width = (i + 1) * w / bars - left - 1;
        view.fillRect({static_cast<int16_t>(bounds_.x + left), static_cast<int16_t>(bounds_.y + h - height),
                       static_cast<int16_t>(width > 0 ? width : 1), static_cast<int16_t>(height)}, mad::Color::On);
        view.pixel(bounds_.x + left, bounds_.y + h - height - 2, mad::Color::On);
      }
    }
  }
 private:
  static void line(mad::Canvas& c, int x0, int y0, int x1, int y1) {
    const int dx = x1 - x0, dy = y1 > y0 ? y1 - y0 : y0 - y1;
    const int sy = y0 < y1 ? 1 : -1;
    int error = dx - dy;
    for (;;) {
      c.pixel(x0, y0, mad::Color::On);
      if (x0 == x1 && y0 == y1) break;
      const int twice = 2 * error;
      if (twice > -dy) { error -= dy; ++x0; }
      if (twice < dx) { error += dx; y0 += sy; }
    }
  }
  uint8_t mode_;
  float phase_ = 0;
  uint32_t lastMs_ = 0;
  bool hasTimestamp_ = false;
};

class Showcase {
 public:
  Showcase() {
    compositor.setFps(60);
    compositor.setTransition(600);
    for (uint8_t i = 0; i < 3; ++i) {
      roots_[i].setSizeMode(mad::SizeMode::Max, mad::SizeMode::Max);
      entrances_[i].setSizeMode(mad::SizeMode::Max, mad::SizeMode::Max);
      roots_[i].add(titles_[i]); roots_[i].add(entrances_[i]); roots_[i].add(switches_[i]);
      ids_[i] = compositor.registerPage(pages_[i]);
    }
    compositor.openPage(ids_[0]);
  }
  void next() {
    if (index_ == 2) forward_ = false;
    if (index_ == 0) forward_ = true;
    if (forward_) compositor.openPage(ids_[++index_]);
    else { compositor.back(); --index_; }
    entrances_[index_].replay();
  }
  void toggle() { switches_[index_].toggle(); }
  void render(mad::Canvas& canvas, uint32_t nowUs, bool autoplay = true) {
    const uint32_t delta = hasTimestamp_ ? nowUs - lastUs_ : 0;
    hasTimestamp_ = true; lastUs_ = nowUs;
    sceneElapsedUs_ += delta; toggleElapsedUs_ += delta;
    if (autoplay && sceneElapsedUs_ >= 6500000UL) { sceneElapsedUs_ = 0; next(); }
    if (autoplay && toggleElapsedUs_ >= 1600000UL) { toggleElapsedUs_ = 0; toggle(); }
    compositor.render(canvas, nowUs);
  }
  mad::Compositor compositor;
 private:
  mad::VerticalLayout roots_[3];
  mad::Page pages_[3] = {mad::Page(roots_[0]), mad::Page(roots_[1]), mad::Page(roots_[2])};
  mad::Label titles_[3] = {mad::Label("LIQUID WAVES"), mad::Label("ORBITAL MOTION"), mad::Label("PULSE SPECTRUM")};
  Visualization scenes_[3] = {Visualization(0), Visualization(1), Visualization(2)};
  mad::Motion entrances_[3] = {mad::Motion(scenes_[0], mad::Entrance::Reveal, 900, 120),
      mad::Motion(scenes_[1], mad::Entrance::Rise, 800, 120),
      mad::Motion(scenes_[2], mad::Entrance::Slide, 800, 120)};
  mad::Switch switches_[3];
  mad::Compositor::PageId ids_[3];
  uint8_t index_ = 0;
  bool forward_ = true;
  bool hasTimestamp_ = false;
  uint32_t lastUs_ = 0, sceneElapsedUs_ = 0, toggleElapsedUs_ = 0;
};

}  // namespace mad_demo
