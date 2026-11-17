#pragma once

#include "matrix.hpp"
#include "content.hpp"

namespace mad {

class RadioButton;

// A fixed-capacity group provides native radio semantics without heap use:
// selecting one member clears the other members in that group.
class RadioGroup {
 public:
  static constexpr uint8_t kMaxButtons = 16;

  bool add(RadioButton& button);
  bool remove(RadioButton& button);
  bool select(RadioButton& button);
  uint8_t count() const { return count_; }

 private:
  RadioButton* buttons_[kMaxButtons]{};
  uint8_t count_ = 0;
};

// A compact mutually-exclusive option. The intrinsic 11x11 footprint is a
// white rounded ring on a black field, with a 9x9 black inner disk.
struct RadioButtonStyle {
  RadioButtonStyle(Color foreground = Color::Off, Color background = Color::On)
      : foreground(foreground), background(background) {}
  Color foreground;
  Color background;
};

class RadioButton final : public IndicatorControl {
 public:
  MAD_DETAIL_COMPONENT_CHAIN(RadioButton)
  MAD_DETAIL_CHAIN_METHOD(RadioButton, ContentControl, setScale, uint8_t)
  MAD_DETAIL_CHAIN_METHOD(RadioButton, ContentControl, setGap, uint8_t)
  using UseHandler = void (*)(void* context);
  static constexpr int16_t kPreferredWidth = 11;
  static constexpr int16_t kPreferredHeight = 11;
  static constexpr int16_t kCircleSize = 11;
  static constexpr int16_t kInnerCircleSize = 9;
  static constexpr int16_t kCenterSize = 5;

  explicit RadioButton(bool selected = false, RadioButtonStyle style = {})
      : IndicatorControl(11,11), selected_(selected), style_(style), group_(nullptr), focused_(false) {}
  RadioButton(SizeSpec size, bool selected = false, RadioButtonStyle style = {})
      : IndicatorControl(size,11,11), selected_(selected), style_(style), group_(nullptr), focused_(false) {}
  RadioButton(Frame frame, bool selected = false, RadioButtonStyle style = {})
      : IndicatorControl(frame,11,11), selected_(selected), style_(style), group_(nullptr), focused_(false) {}
  ~RadioButton() override { if (group_) group_->remove(*this); }

  bool selected() const { return selected_; }
  RadioButton& setSelected(bool selected) & {
    if (selected && group_) group_->select(*this);
    else selected_ = selected;
   return *this; }
  RadioButton&& setSelected(bool selected) && { setSelected(selected); return static_cast<RadioButton&&>(*this); }
  void select() { setSelected(true); }

  bool join(RadioGroup& group) {
    if (group_ == &group) return true;
    if (group_) group_->remove(*this);
    group_ = &group;
    if (group_->add(*this)) return true;
    group_ = nullptr;
    return false;
  }
  void leaveGroup() { if (group_) group_->remove(*this); }

  RadioButton& setStyle(RadioButtonStyle style) & { style_ = style; return *this; }
  RadioButton&& setStyle(RadioButtonStyle style) && { setStyle(style); return static_cast<RadioButton&&>(*this); }
  RadioButtonStyle style() const { return style_; }
  MAD_DETAIL_USE_CALLBACK(RadioButton)
  MAD_DETAIL_CLICK_ALIAS(RadioButton)

  bool interactive() const override { return true; }
  uint8_t focusCornerCut() const override { return hasContent() ? 0 : static_cast<uint8_t>(2*scale()); }
  bool focused() const override { return focused_; }
  void setFocused(bool focused) override { focused_ = focused; }

  bool handle(Action action) override {
    if (!enabled_ || action != Action::Use) return false;
    setSelected(true);
    callback_();
    return true;
  }

  void draw(Canvas& canvas) const override {
    if (!visible_ || bounds_.width <= 0 || bounds_.height <= 0) return;

    drawContentBackground(canvas,style_.foreground,focused_);

    static const MatrixPixel kUnselected[11][11] = {
        {N, N, N, WF, WF, WF, WF, WF, N, N, N},
        {N, N, WF, B, B, B, B, B, WF, N, N},
        {N, WF, B, B, B, B, B, B, B, WF, N},
        {WF, B, B, B, B, B, B, B, B, B, WF},
        {WF, B, B, B, B, B, B, B, B, B, WF},
        {WF, B, B, B, B, B, B, B, B, B, WF},
        {WF, B, B, B, B, B, B, B, B, B, WF},
        {WF, B, B, B, B, B, B, B, B, B, WF},
        {N, WF, B, B, B, B, B, B, B, WF, N},
        {N, N, WF, B, B, B, B, B, WF, N, N},
        {N, N, N, WF, WF, WF, WF, WF, N, N, N},
    };
    static const MatrixPixel kSelected[11][11] = {
        {N, N, N, WF, WF, WF, WF, WF, N, N, N},
        {N, N, WF, B, B, B, B, B, WF, N, N},
        {N, WF, B, B, B, B, B, B, B, WF, N},
        {WF, B, B, B, W, W, W, B, B, B, WF},
        {WF, B, B, W, W, W, W, W, B, B, WF},
        {WF, B, B, W, W, W, W, W, B, B, WF},
        {WF, B, B, W, W, W, W, W, B, B, WF},
        {WF, B, B, B, W, W, W, B, B, B, WF},
        {N, WF, B, B, B, B, B, B, B, WF, N},
        {N, N, WF, B, B, B, B, B, WF, N, N},
        {N, N, N, WF, WF, WF, WF, WF, N, N, N},
    };
    ContentCanvas artwork(canvas,indicatorBounds(),scale());
    const int16_t x = 0,y = 0;
    if (selected_)
      drawMatrix(artwork, x, y, kSelected, focused_,
                 style_.foreground, style_.background);
    else
      drawMatrix(artwork, x, y, kUnselected, focused_,
                 style_.foreground, style_.background);
    drawContent(canvas,focused_);
  }

 private:

  bool selected_;
  RadioButtonStyle style_;
  detail::UseCallback callback_;
  RadioGroup* group_;
  bool focused_;

  friend class RadioGroup;
};

inline bool RadioGroup::add(RadioButton& button) {
  for (uint8_t i = 0; i < count_; ++i)
    if (buttons_[i] == &button) return true;
  if (count_ == kMaxButtons) return false;
  if (button.group_ && button.group_ != this) button.group_->remove(button);
  buttons_[count_++] = &button;
  button.group_ = this;
  if (button.selected_) select(button);
  return true;
}

inline bool RadioGroup::remove(RadioButton& button) {
  for (uint8_t i = 0; i < count_; ++i) {
    if (buttons_[i] != &button) continue;
    for (uint8_t j = i + 1; j < count_; ++j) buttons_[j - 1] = buttons_[j];
    buttons_[--count_] = nullptr;
    button.group_ = nullptr;
    return true;
  }
  if (button.group_ == this) button.group_ = nullptr;
  return false;
}

inline bool RadioGroup::select(RadioButton& button) {
  bool found = false;
  for (uint8_t i = 0; i < count_; ++i)
    if (buttons_[i] == &button) { found = true; break; }
  if (!found) return false;
  for (uint8_t i = 0; i < count_; ++i) {
    const bool selected = buttons_[i] == &button;
    buttons_[i]->selected_ = selected;
  }
  return true;
}
}  // namespace mad
