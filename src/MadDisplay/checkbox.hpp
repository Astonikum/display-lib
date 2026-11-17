#pragma once

#include "matrix.hpp"
#include "content.hpp"

namespace mad {

// A square two-state control. Its intrinsic 11x11 footprint leaves a one
// pixel surround around the 9x9 box; a checked box uses the seven-pixel mark
// from the reference artwork.
struct CheckboxStyle {
  CheckboxStyle(Color foreground = Color::Off, Color background = Color::On)
      : foreground(foreground), background(background) {}
  Color foreground;
  Color background;
};

class Checkbox final : public IndicatorControl {
 public:
  MAD_DETAIL_COMPONENT_CHAIN(Checkbox)
  MAD_DETAIL_CHAIN_METHOD(Checkbox, ContentControl, setScale, uint8_t)
  MAD_DETAIL_CHAIN_METHOD(Checkbox, ContentControl, setGap, uint8_t)
  using UseHandler = void (*)(void* context);
  static constexpr int16_t kPreferredWidth = 11;
  static constexpr int16_t kPreferredHeight = 11;
  static constexpr int16_t kBoxSize = 9;

  explicit Checkbox(bool checked = false, CheckboxStyle style = {})
      : IndicatorControl(11,11), checked_(checked), style_(style), focused_(false) {}
  Checkbox(SizeSpec size, bool checked = false, CheckboxStyle style = {})
      : IndicatorControl(size,11,11), checked_(checked), style_(style), focused_(false) {}
  Checkbox(Frame frame, bool checked = false, CheckboxStyle style = {})
      : IndicatorControl(frame,11,11), checked_(checked), style_(style), focused_(false) {}

  bool checked() const { return checked_; }
  Checkbox& setChecked(bool checked) & { checked_ = checked; return *this; }
  Checkbox&& setChecked(bool checked) && { setChecked(checked); return static_cast<Checkbox&&>(*this); }
  void toggle() { checked_ = !checked_; }

  Checkbox& setStyle(CheckboxStyle style) & { style_ = style; return *this; }
  Checkbox&& setStyle(CheckboxStyle style) && { setStyle(style); return static_cast<Checkbox&&>(*this); }
  CheckboxStyle style() const { return style_; }
  MAD_DETAIL_USE_CALLBACK(Checkbox)
  MAD_DETAIL_CLICK_ALIAS(Checkbox)

  bool interactive() const override { return true; }
  uint8_t focusCornerCut() const override { return hasContent() ? 0 : static_cast<uint8_t>(0*scale()); }
  bool focused() const override { return focused_; }
  void setFocused(bool focused) override { focused_ = focused; }

  bool handle(Action action) override {
    if (!enabled_ || action != Action::Use) return false;
    toggle();
    callback_();
    return true;
  }

  void draw(Canvas& canvas) const override {
    if (!visible_ || bounds_.width <= 0 || bounds_.height <= 0) return;

    drawContentBackground(canvas,style_.foreground,focused_);

    static const MatrixPixel kUnchecked[11][11] = {
        {N, WF, WF, WF, WF, WF, WF, WF, WF, WF, N},
        {WF, B, B, B, B, B, B, B, B, B, WF},
        {WF, B, B, B, B, B, B, B, B, B, WF},
        {WF, B, B, B, B, B, B, B, B, B, WF},
        {WF, B, B, B, B, B, B, B, B, B, WF},
        {WF, B, B, B, B, B, B, B, B, B, WF},
        {WF, B, B, B, B, B, B, B, B, B, WF},
        {WF, B, B, B, B, B, B, B, B, B, WF},
        {WF, B, B, B, B, B, B, B, B, B, WF},
        {WF, B, B, B, B, B, B, B, B, B, WF},
        {N, WF, WF, WF, WF, WF, WF, WF, WF, WF, N},
    };
    static const MatrixPixel kChecked[11][11] = {
        {N, WF, WF, WF, WF, WF, WF, WF, WF, WF, N},
        {WF, B, B, B, B, B, B, B, B, B, WF},
        {WF, B, B, B, B, B, B, B, B, B, WF},
        {WF, B, B, B, B, B, B, B, W, B, WF},
        {WF, B, B, B, B, B, B, W, B, B, WF},
        {WF, B, W, B, B, B, W, B, B, B, WF},
        {WF, B, B, W, B, W, B, B, B, B, WF},
        {WF, B, B, B, W, B, B, B, B, B, WF},
        {WF, B, B, B, B, B, B, B, B, B, WF},
        {WF, B, B, B, B, B, B, B, B, B, WF},
        {N, WF, WF, WF, WF, WF, WF, WF, WF, WF, N},
    };
    ContentCanvas artwork(canvas,indicatorBounds(),scale());
    const int16_t x = 0,y = 0;
    if (checked_)
      drawMatrix(artwork, x, y, kChecked, focused_, style_.foreground, style_.background);
    else
      drawMatrix(artwork, x, y, kUnchecked, focused_, style_.foreground, style_.background);
    drawContent(canvas,focused_);
  }

 private:
  bool checked_;
  CheckboxStyle style_;
  detail::UseCallback callback_;
  bool focused_;
};
}  // namespace mad
