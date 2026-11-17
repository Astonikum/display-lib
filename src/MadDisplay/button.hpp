#pragma once

#include <string.h>

#include "content.hpp"
#include "label.hpp"

namespace mad {

struct ButtonStyle {
  ButtonStyle(Color foreground = Color::On, Color background = Color::Off,
              uint8_t textScale = 1, uint8_t padding = 1, bool filled = false,
              const BitmapFont* font = nullptr)
      : foreground(foreground), background(background), textScale(textScale), padding(padding),
        filled(filled), font(font) {}
  Color foreground;
  Color background;
  uint8_t textScale;
  uint8_t padding; // Base units; text, padding, border and corners share textScale.
  bool filled;
  const BitmapFont* font;
};

class Button final : public ContentControl {
 public:
  MAD_DETAIL_COMPONENT_CHAIN(Button)
  using UseHandler = void (*)(void* context);

  explicit Button(ButtonStyle style = {}) : label_(""),style_(style) { configureLabel(); }
  explicit Button(Component& content,ButtonStyle style = {}) : Button(style) { setContent(content); }
  Button(Frame frame,Component& content,ButtonStyle style = {}) : Button(content,style) { setFrame(frame); }
  Button(SizeSpec size,Component& content,ButtonStyle style = {}) : Button(content,style) { setSize(size); }
  // Source-compatible convenience API; the text is a real Label in the content slot.
  explicit Button(const char* text,ButtonStyle style = {}) : Button(style) { setText(text); }
  Button(SizeSpec size,const char* text,ButtonStyle style = {}) : Button(text,style) { setSize(size); }
  Button(Frame frame,const char* text,ButtonStyle style = {}) : Button(text,style) { setFrame(frame); }
  template<class T> explicit Button(T&& content,ButtonStyle style = {},
      typename detail::EnableIf<!detail::IsLvalue<T>::value && !detail::IsSame<detail::Stored<T>,Button>::value>::Type* = nullptr,
      decltype(&static_cast<Component&>(content)) = nullptr) : Button(style) {
    setContent(detail::forward<T>(content));
  }
  template<class Text> explicit Button(const Text& text,ButtonStyle style = {},decltype(text.c_str()) = nullptr)
      : Button(style) { setText(text); }
  Button(Button&& other)
      : ContentControl(other),label_(static_cast<Label&&>(other.label_)),style_(other.style_),callback_(other.callback_),focused_(other.focused_) {
    if (other.content() == &other.label_) setContent(label_);
  }
  Button(const Button& other)
      : ContentControl(other),label_(other.label_),style_(other.style_),callback_(other.callback_),focused_(other.focused_) {
    if (other.content() == &other.label_) setContent(label_);
  }
  Button& operator=(const Button& other) {
    if (this == &other) return *this;
    ContentControl::operator=(other); label_ = other.label_; style_ = other.style_;
    callback_ = other.callback_; focused_ = other.focused_;
    if (other.content() == &other.label_) setContent(label_);
    return *this;
  }
  Button& setScale(uint8_t value) & { style_.textScale = value ? value : 1; ContentControl::setScale(value); return *this; }
  Button&& setScale(uint8_t value) && { setScale(value); return static_cast<Button&&>(*this); }
  template<class Text> Button& setText(const Text& text) & { label_.setText(text); setContent(label_); return *this; }
  template<class Text> Button&& setText(const Text& text) && { setText(text); return static_cast<Button&&>(*this); }
  Button& setStyle(ButtonStyle style) & { style_ = style; configureLabel(); return *this; }
  Button&& setStyle(ButtonStyle style) && { setStyle(style); return static_cast<Button&&>(*this); }
  Button& setFont(const BitmapFont* font) & { style_.font = font; configureLabel(); return *this; }
  Button&& setFont(const BitmapFont* font) && { setFont(font); return static_cast<Button&&>(*this); }
  Size preferredSize() const override {
    const Size content = intrinsicContentSize(); const int16_t inset = (style_.padding+1)*scale();
    return {coordinate(int32_t(scaled(content.width))+inset*2),coordinate(int32_t(scaled(content.height))+inset*2)};
  }
  Rect contentBounds() const override {
    const int16_t inset = (style_.padding+1)*scale();
    return {int16_t(bounds_.x+inset),int16_t(bounds_.y+inset),int16_t(bounds_.width > inset*2 ? bounds_.width-inset*2 : 0),int16_t(bounds_.height > inset*2 ? bounds_.height-inset*2 : 0)};
  }
  MAD_DETAIL_USE_CALLBACK(Button)
  MAD_DETAIL_CLICK_ALIAS(Button)
  bool interactive() const override { return true; }
  Rect focusBounds() const override {
    const int16_t border = scale();
    return {static_cast<int16_t>(bounds_.x + border),static_cast<int16_t>(bounds_.y + border),
        static_cast<int16_t>(bounds_.width > border*2 ? bounds_.width - border*2 : 0),
        static_cast<int16_t>(bounds_.height > border*2 ? bounds_.height - border*2 : 0)};
  }
  bool focused() const override { return focused_; }
  void setFocused(bool focused) override { focused_ = focused; }
  bool handle(Action action) override {
    if (!enabled_ || action != Action::Use) return false;
    callback_();
    return true;
  }
  void draw(Canvas& canvas) const override {
    if (!visible_) return;
    const uint8_t borderScale = scale();
    if (focused_) {
      canvas.fillRect(bounds_,inverted(style_.filled ? style_.foreground : style_.background));
      cutCorners(canvas,bounds_,style_.background,borderScale);
    } else if (style_.filled) {
      canvas.fillRect(bounds_,style_.foreground); cutCorners(canvas,bounds_,style_.background,borderScale);
    }
    strokeCutCorners(canvas,bounds_,style_.foreground,borderScale);
    drawContent(canvas,focused_);
  }

 private:
  static void fixedRect(Canvas& canvas,Rect rect,Color color) {
    for (int16_t y = 0; y < rect.height; ++y)
      for (int16_t x = 0; x < rect.width; ++x)
        canvas.fixedPixel(static_cast<int16_t>(rect.x+x),static_cast<int16_t>(rect.y+y),color);
  }
  static void cutCorners(Canvas& canvas,Rect rect,Color replacement,uint8_t scale) {
    if (rect.width <= 0 || rect.height <= 0) return;
    const int16_t w = rect.width < scale ? rect.width : scale, h = rect.height < scale ? rect.height : scale;
    fixedRect(canvas,{rect.x,rect.y,w,h},replacement);
    fixedRect(canvas,{static_cast<int16_t>(rect.x+rect.width-w),rect.y,w,h},replacement);
    fixedRect(canvas,{rect.x,static_cast<int16_t>(rect.y+rect.height-h),w,h},replacement);
    fixedRect(canvas,{static_cast<int16_t>(rect.x+rect.width-w),static_cast<int16_t>(rect.y+rect.height-h),w,h},replacement);
  }

  static void strokeCutCorners(Canvas& canvas,Rect rect,Color color,uint8_t scale) {
    if (rect.width <= scale*2 || rect.height <= scale*2) return;
    const int16_t innerWidth = rect.width-scale*2, innerHeight = rect.height-scale*2;
    fixedRect(canvas,{static_cast<int16_t>(rect.x+scale),rect.y,innerWidth,scale},color);
    fixedRect(canvas,{static_cast<int16_t>(rect.x+scale),static_cast<int16_t>(rect.y+rect.height-scale),innerWidth,scale},color);
    fixedRect(canvas,{rect.x,static_cast<int16_t>(rect.y+scale),scale,innerHeight},color);
    fixedRect(canvas,{static_cast<int16_t>(rect.x+rect.width-scale),static_cast<int16_t>(rect.y+scale),scale,innerHeight},color);
  }

  void configureLabel() {
    setScale(style_.textScale);
    label_.setStyle(LabelStyle(style_.filled ? style_.background : style_.foreground,1,HorizontalAlign::Left,VerticalAlign::Center,style_.font));
  }
  Label label_;
  ButtonStyle style_;
  detail::UseCallback callback_;
  bool focused_ = false;
};
}  // namespace mad
