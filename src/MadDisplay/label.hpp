#pragma once

#include <string.h>

#include "text.hpp"

namespace mad {

struct LabelStyle {
  LabelStyle(Color color = Color::On, uint8_t scale = 1,
             HorizontalAlign horizontalAlign = HorizontalAlign::Left,
             VerticalAlign verticalAlign = VerticalAlign::Center,
             const BitmapFont* font = nullptr)
      : color(color), scale(scale), horizontalAlign(horizontalAlign), verticalAlign(verticalAlign), font(font) {}
  Color color;
  uint8_t scale;
  HorizontalAlign horizontalAlign;
  VerticalAlign verticalAlign;
  const BitmapFont* font;
};

class Label final : public Component {
 public:
  MAD_DETAIL_COMPONENT_CHAIN(Label)
  explicit Label(const char* value, LabelStyle style = {})
      : Component(), value_(value), style_(style) {}
  Label(SizeSpec size, const char* value, LabelStyle style = {})
      : Component(size), value_(value), style_(style) {}
  Label(Frame frame, const char* value, LabelStyle style = {})
      : Component(frame), value_(value), style_(style) {}

  template<class Text> explicit Label(const Text& value,LabelStyle style = {},decltype(value.c_str()) = nullptr)
      : Label(value.c_str(),style) {}
  Label& setText(const char* value) & { value_.assign(value); textLayout_ = nullptr; return *this; }
  Label&& setText(const char* value) && { setText(value); return static_cast<Label&&>(*this); }
  template<class Text> auto setText(const Text& value) & -> decltype(value.c_str(),*this) { return setText(value.c_str()); }
  template<class Text> auto setText(const Text& value) && -> decltype(value.c_str(),static_cast<Label&&>(*this)) {
    setText(value.c_str()); return static_cast<Label&&>(*this);
  }
  template<class Number> typename detail::EnableIf<detail::IsInteger<Number>::value,Label&>::Type setText(Number value) & {
    if (Number(-1) < Number(0)) value_.signedNumber(static_cast<int64_t>(value));
    else value_.number(static_cast<uint64_t>(value));
    textLayout_ = nullptr; return *this;
  }
  template<class Number> typename detail::EnableIf<detail::IsInteger<Number>::value,Label&&>::Type setText(Number value) && {
    setText(value); return static_cast<Label&&>(*this);
  }
  Label& setText(double value,uint8_t decimals = 2) & {
    value_.decimal(value,decimals); textLayout_ = nullptr; return *this;
  }
  Label&& setText(double value,uint8_t decimals = 2) && {
    setText(value,decimals); return static_cast<Label&&>(*this);
  }
  const char* text() const { return value_.c_str(); }
  bool valid() const override { return Component::valid() && value_.valid(); }
  Label& setTextLayout(const TextLayout& layout) & { textLayout_ = &layout; return *this; }
  Label&& setTextLayout(const TextLayout& layout) && { setTextLayout(layout); return static_cast<Label&&>(*this); }
  const TextLayout* textLayout() const { return textLayout_; }
  Label& setStyle(LabelStyle style) & { if (style.font != style_.font) textLayout_ = nullptr; style_ = style; return *this; }
  Label&& setStyle(LabelStyle style) && { setStyle(style); return static_cast<Label&&>(*this); }
  Label& setHorizontalAlign(HorizontalAlign align) & { style_.horizontalAlign = align; return *this; }
  Label&& setHorizontalAlign(HorizontalAlign align) && { setHorizontalAlign(align); return static_cast<Label&&>(*this); }
  Label& setVerticalAlign(VerticalAlign align) & { style_.verticalAlign = align; return *this; }
  Label&& setVerticalAlign(VerticalAlign align) && { setVerticalAlign(align); return static_cast<Label&&>(*this); }
  Label& setFont(const BitmapFont* font) & { style_.font = font; textLayout_ = nullptr; return *this; }
  Label&& setFont(const BitmapFont* font) && { setFont(font); return static_cast<Label&&>(*this); }
  Label& setAlignment(HorizontalAlign horizontal, VerticalAlign vertical) & {
    style_.horizontalAlign = horizontal;
    style_.verticalAlign = vertical;
   return *this; }
  Label&& setAlignment(HorizontalAlign horizontal, VerticalAlign vertical) && { setAlignment(horizontal,vertical); return static_cast<Label&&>(*this); }
  Size preferredSize() const override {
    const uint8_t scale = style_.scale ? style_.scale : 1;
    if (textLayout_) return {int16_t(int32_t(textLayout_->size.width)*scale > 32767 ? 32767 : textLayout_->size.width*scale),
        int16_t(int32_t(textLayout_->size.height)*scale > 32767 ? 32767 : textLayout_->size.height*scale)};
    if (style_.font) return bitmapTextSize(value_.c_str(), *style_.font, scale);
    return defaultTextSize(value_.c_str(),scale);
  }
  void draw(Canvas& canvas) const override {
    if (!visible_) return;
    const uint8_t scale = style_.scale ? style_.scale : 1;
    const Size text = preferredSize();
    int16_t x = bounds_.x;
    int16_t y = bounds_.y;
    if (style_.horizontalAlign == HorizontalAlign::Center)
      x = static_cast<int16_t>(bounds_.x + (bounds_.width - text.width) / 2);
    else if (style_.horizontalAlign == HorizontalAlign::Right)
      x = static_cast<int16_t>(bounds_.x + bounds_.width - text.width);
    if (style_.verticalAlign == VerticalAlign::Center)
      y = static_cast<int16_t>(bounds_.y + (bounds_.height - text.height) / 2);
    else if (style_.verticalAlign == VerticalAlign::Bottom)
      y = static_cast<int16_t>(bounds_.y + bounds_.height - text.height);
    if (textLayout_) drawTextLayout(canvas,*textLayout_,x,y,scale,style_.color);
    else if (style_.font)
      canvas.text(x, y, value_.c_str(), *style_.font, scale, style_.color);
    else
      canvas.text(x, y, value_.c_str(), scale, style_.color);
  }

 private:
  const TextLayout* textLayout_ = nullptr;
  detail::TextValue value_;
  LabelStyle style_;
};
}  // namespace mad
