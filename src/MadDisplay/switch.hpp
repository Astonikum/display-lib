#pragma once

#include "matrix.hpp"
#include "content.hpp"

namespace mad {

// A compact two-state control. Its artwork is deliberately fixed to the
// low-resolution OLED grid: a 20x11 component containing an 18x9 rounded
// track, a 7x7 thumb, and the conventional O/I state marks.
struct SwitchStyle {
  // Focus swaps track/thumb interiors. Both outlines stay white in every state.
  SwitchStyle(Color onTrack = Color::Off, Color offTrack = Color::Off,
              Color onThumb = Color::On, Color offThumb = Color::On)
      : onTrack(onTrack), offTrack(offTrack), onThumb(onThumb),
        offThumb(offThumb) {}
  Color onTrack;
  Color offTrack;
  Color onThumb;
  Color offThumb;
};

class Switch final : public IndicatorControl {
 public:
  MAD_DETAIL_COMPONENT_CHAIN(Switch)
  MAD_DETAIL_CHAIN_METHOD(Switch, ContentControl, setScale, uint8_t)
  MAD_DETAIL_CHAIN_METHOD(Switch, ContentControl, setGap, uint8_t)
  using UseHandler = void (*)(void* context);
  static constexpr int16_t kPreferredWidth = 20;
  static constexpr int16_t kPreferredHeight = 11;
  static constexpr int16_t kThumbSize = 7;

  explicit Switch(bool on = false, SwitchStyle style = {})
      : IndicatorControl(20,11), on_(on), style_(style), focused_(false) {}
  Switch(SizeSpec size, bool on = false, SwitchStyle style = {})
      : IndicatorControl(size,20,11), on_(on), style_(style), focused_(false) {}
  Switch(Frame frame, bool on = false, SwitchStyle style = {})
      : IndicatorControl(frame,20,11), on_(on), style_(style), focused_(false) {}

  bool on() const { return on_; }
  Switch& setOn(bool on) & { on_ = on; return *this; }
  Switch&& setOn(bool on) && { setOn(on); return static_cast<Switch&&>(*this); }
  void toggle() { on_ = !on_; }

  Switch& setStyle(SwitchStyle style) & { style_ = style; return *this; }
  Switch&& setStyle(SwitchStyle style) && { setStyle(style); return static_cast<Switch&&>(*this); }
  SwitchStyle style() const { return style_; }
  MAD_DETAIL_USE_CALLBACK(Switch)
  MAD_DETAIL_CLICK_ALIAS(Switch)

  bool interactive() const override { return true; }
  uint8_t focusCornerCut() const override { return hasContent() ? 0 : static_cast<uint8_t>(1*scale()); }
  bool focused() const override { return focused_; }
  void setFocused(bool focused) override { focused_ = focused; }

  bool handle(Action action) override {
    if (!enabled_ || action != Action::Use) return false;
    toggle();
    callback_();
    return true;
  }

  Switch& setAnimationDuration(uint16_t milliseconds) & { durationMs_ = milliseconds; return *this; }
  Switch&& setAnimationDuration(uint16_t milliseconds) && { setAnimationDuration(milliseconds); return static_cast<Switch&&>(*this); }
  void update(uint32_t nowMs) override {
    ContentControl::update(nowMs);
    animated_ = true;
    if (motionTarget_ != on_) {
      motion_.retarget(on_ ? 9.0f : 0.0f, nowMs, durationMs_, Easing::CubicInOut);
      motionTarget_ = on_;
    }
    thumbPosition_ = motion_.value(nowMs);
  }

  void draw(Canvas& canvas) const override {
    if (!visible_ || bounds_.width <= 0 || bounds_.height <= 0) return;

    const Color track = on_ ? style_.onTrack : style_.offTrack;
    drawContentBackground(canvas,track,focused_);
    const Color thumb = on_ ? style_.onThumb : style_.offThumb;
    ContentCanvas artwork(canvas,indicatorBounds(),scale());
    const int16_t x = 0,y = 0;
    const int16_t offset = animated_ ? static_cast<int16_t>(thumbPosition_ + 0.5f) : (on_ ? 9 : 0);
    const Color trackColor = focused_ ? inverted(track) : track;
    const Color thumbColor = focused_ ? inverted(thumb) : thumb;
    // Preserve the fixed white perimeter; only the thumb moves inside it.
    for (int16_t row = 0; row < 11; ++row) {
      const int16_t cut = row == 0 || row == 10 ? 2 : (row == 1 || row == 9 ? 1 : 0);
      for (int16_t col = cut; col < 20 - cut; ++col) {
        const bool border = row == 0 || row == 10 || col == cut || col == 19 - cut;
        if (border) artwork.fixedPixel(x + col,y + row,Color::On);
        else artwork.pixel(x + col,y + row,trackColor);
      }
    }
    for (int16_t row = 0; row < 7; ++row)
      for (int16_t col = 0; col < 7; ++col) {
        if ((row == 0 || row == 6) && (col == 0 || col == 6)) continue;
        const bool mark = on_ ? (col == 3 && row >= 2 && row <= 4)
            : (row >= 2 && row <= 4 && col >= 2 && col <= 4 && !(row == 3 && col == 3));
        const bool border = row == 0 || row == 6 || col == 0 || col == 6;
        if (border) artwork.fixedPixel(x + 2 + offset + col,y + 2 + row,Color::On);
        else artwork.pixel(x + 2 + offset + col,y + 2 + row,mark ? trackColor : thumbColor);
      }
    drawContent(canvas,focused_);
  }

 private:
  static Color inverted(Color color) { return mad::inverted(color); }
  bool on_;
  SwitchStyle style_;
  detail::UseCallback callback_;
  bool focused_;
  Tween motion_{on_ ? 9.0f : 0.0f};
  float thumbPosition_ = on_ ? 9.0f : 0.0f;
  bool motionTarget_ = on_;
  bool animated_ = false;
  uint16_t durationMs_ = 220;
};
}  // namespace mad
