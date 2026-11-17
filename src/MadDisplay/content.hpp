#pragma once
#include "core.hpp"

namespace mad {
// Local logical coordinates, one integer scale for all artwork and spacing.
class ContentCanvas final : public Canvas {
 public:
  ContentCanvas(Canvas& target,Rect frame,uint8_t scale = 1,bool invert = false)
      : target_(target),frame_(frame),scale_(scale ? scale : 1),invert_(invert) {}
  Size size() const override { return {int16_t(frame_.width/scale_),int16_t(frame_.height/scale_)}; }
  PixelFormat format() const override { return target_.format(); }
  Rect viewport() const override {
    const Rect clip = target_.viewport();
    const int32_t left = clip.x > frame_.x ? clip.x-frame_.x : 0;
    const int32_t top = clip.y > frame_.y ? clip.y-frame_.y : 0;
    const int32_t right = int32_t(clip.x)+clip.width < int32_t(frame_.x)+frame_.width ? int32_t(clip.x)+clip.width-frame_.x : frame_.width;
    const int32_t bottom = int32_t(clip.y)+clip.height < int32_t(frame_.y)+frame_.height ? int32_t(clip.y)+clip.height-frame_.y : frame_.height;
    return {int16_t(left/scale_),int16_t(top/scale_),int16_t(right > left ? (right+scale_-1)/scale_-left/scale_ : 0),
        int16_t(bottom > top ? (bottom+scale_-1)/scale_-top/scale_ : 0)};
  }
  void clear(Color color = Color::Off) override { fillRect({0,0,size().width,size().height},color); }
  void pixel(int16_t x,int16_t y,Color color) override { fillRect({x,y,1,1},color); }
  void fixedPixel(int16_t x,int16_t y,Color color) override {
    if (x < 0 || y < 0 || x >= size().width || y >= size().height) return;
    const int32_t px = int32_t(frame_.x)+int32_t(x)*scale_,py = int32_t(frame_.y)+int32_t(y)*scale_;
    for (uint8_t dy = 0; dy < scale_; ++dy) for (uint8_t dx = 0; dx < scale_; ++dx)
      target_.fixedPixel(int16_t(px+dx),int16_t(py+dy),color);
  }
  void fillSpan(int16_t x,int16_t y,int16_t length,Color color) override { fillRect({x,y,length,1},color); }
  void fillRect(Rect rect,Color color) override {
    const int32_t left = rect.x < 0 ? 0 : rect.x,top = rect.y < 0 ? 0 : rect.y;
    const int32_t right = int32_t(rect.x)+rect.width < size().width ? int32_t(rect.x)+rect.width : size().width;
    const int32_t bottom = int32_t(rect.y)+rect.height < size().height ? int32_t(rect.y)+rect.height : size().height;
    if (right <= left || bottom <= top) return;
    target_.fillRect({int16_t(frame_.x+left*scale_),int16_t(frame_.y+top*scale_),int16_t((right-left)*scale_),int16_t((bottom-top)*scale_)},invert_ ? inverted(color) : color);
  }
  void text(int16_t x,int16_t y,const char* value,uint8_t scale,Color color) override { drawDefaultText(*this,x,y,value,scale,color); }
  void present() override {}
 private:
  Canvas& target_; Rect frame_; uint8_t scale_; bool invert_;
};

// One content slot: borrow named content or own a temporary composition.
// The owner is the only focus/action target, even when content is itself composite.
class ContentControl : public Component {
 public:
  ContentControl() = default;
  explicit ContentControl(Frame frame) : Component(frame) {}
  explicit ContentControl(SizeSpec size) : Component(size) {}
  bool setContent(Component& content) {
    Rect descendant;
    if (&content == this || content.locate(*this,descendant)) return false;
    if (content_ == &content) return true;
    content_ = &content; content_->setScrollAnimation(scrollDurationMs_); return true;
  }
  template<class T> typename detail::EnableIf<!detail::IsLvalue<T>::value,bool>::Type setContent(T&& content) {
    Rect descendant;
    if (content.locate(*this,descendant)) return false;
    ComponentSlot slot;
    if (!slot.own(detail::forward<T>(content))) return false;
    content_ = slot; content_->setScrollAnimation(scrollDurationMs_); return true;
  }
  bool valid() const override { return Component::valid() && (!content_ || content_->valid()); }
  bool add(Component& content) { return !content_ && setContent(content); }
  template<class T> typename detail::EnableIf<!detail::IsLvalue<T>::value,bool>::Type add(T&& content) {
    return !content_ && setContent(detail::forward<T>(content));
  }
  void clearContent() { content_ = nullptr; }
  Component* content() const { return content_; }
  bool hasContent() const { return content_ && content_->visible(); }
  void setScale(uint8_t scale) { scale_ = scale ? scale : 1; }
  uint8_t scale() const { return scale_; }
  void setGap(uint8_t gap) { gap_ = gap; }
  uint8_t gap() const { return gap_; }
  virtual Rect contentBounds() const { return bounds_; }
  Size preferredSize() const override {
    const Size size = intrinsicContentSize(); return {scaled(size.width),scaled(size.height)};
  }
  void layout(Rect parent) override { Component::layout(parent); layoutChildren(); }
  void update(uint32_t nowMs) override { if (hasContent()) content_->update(nowMs); }
  void setScrollAnimation(uint16_t ms) override { scrollDurationMs_ = ms; if (content_) content_->setScrollAnimation(ms); }
  bool scrollAt(Point position,Point delta) override {
    const Rect frame = contentBounds();
    if (!visible_ || !enabled_ || !hasContent() || !containsPoint(frame,position)) return false;
    return content_->scrollAt({int16_t((position.x-frame.x)/scale_),int16_t((position.y-frame.y)/scale_)},
        {int16_t(delta.x ? (delta.x/scale_ ? delta.x/scale_ : delta.x > 0 ? 1 : -1) : 0),
         int16_t(delta.y ? (delta.y/scale_ ? delta.y/scale_ : delta.y > 0 ? 1 : -1) : 0)});
  }
  bool locate(const Component& target,Rect& result,bool destination = false) const override {
    if (Component::locate(target,result,destination)) return true;
    if (!content_ || !content_->locate(target,result,destination)) return false;
    const Rect frame = contentBounds();
    result = {coordinate(int32_t(frame.x)+int32_t(result.x)*scale_),coordinate(int32_t(frame.y)+int32_t(result.y)*scale_),scaled(result.width),scaled(result.height)};
    return true;
  }
  bool ensureVisible(const Component& target) override { return content_ && (content_ == &target || content_->ensureVisible(target)); }
 protected:
  Size intrinsicContentSize() const {
    if (!hasContent()) return {0,0};
    return content_->intrinsicSize();
  }
  void layoutChildren() override {
    if (!hasContent()) return;
    const Rect frame = contentBounds(); const int16_t width = frame.width/scale_,height = frame.height/scale_;
    Size wanted = content_->requestedSize({0,0,width,height});
    if (content_->widthIsMax()) wanted.width = width;
    if (content_->heightIsMax()) wanted.height = height;
    wanted.width = wanted.width < width ? wanted.width : width;
    wanted.height = wanted.height < height ? wanted.height : height;
    if (wanted.width < 0) wanted.width = 0;
    if (wanted.height < 0) wanted.height = 0;
    const Rect cell{0,int16_t((height-wanted.height)/2),wanted.width,wanted.height};
    Rect placed = cell;
    if (content_->hasFrame()) {
      placed.x = coordinate(int32_t(placed.x)+resolveLength(content_->frame().x,width));
      placed.y = coordinate(int32_t(placed.y)+resolveLength(content_->frame().y,height));
    }
    content_->setResolvedBounds(placed); content_->layoutChildren();
  }
  void drawContent(Canvas& canvas,bool invert) const {
    if (!hasContent()) return;
    ContentCanvas local(canvas,contentBounds(),scale_,invert); content_->draw(local);
  }
  static int16_t coordinate(int32_t value) { return value > 32767 ? 32767 : value < -32768 ? -32768 : int16_t(value); }
  int16_t scaled(int32_t value) const { return coordinate(value*scale_); }
 private:
  ComponentSlot content_;
  uint8_t scale_ = 1,gap_ = kDefaultGap;
  uint16_t scrollDurationMs_ = 280;
};

class IndicatorControl : public ContentControl {
 public:
  IndicatorControl(uint8_t width,uint8_t height) : width_(width),height_(height) {}
  IndicatorControl(Frame frame,uint8_t width,uint8_t height) : ContentControl(frame),width_(width),height_(height) {}
  IndicatorControl(SizeSpec size,uint8_t width,uint8_t height) : ContentControl(size),width_(width),height_(height) {}
  Size preferredSize() const override {
    const Size content = intrinsicContentSize();
    return {scaled(width_+(hasContent() ? gap()+content.width : 0)),scaled(content.height > height_ ? content.height : height_)};
  }
  Rect indicatorBounds() const {
    const int16_t width = scaled(width_),height = scaled(height_);
    return {int16_t(bounds_.x+(hasContent() ? 0 : (bounds_.width-width)/2)),int16_t(bounds_.y+(bounds_.height-height)/2),width,height};
  }
  Rect contentBounds() const override {
    const int16_t inset = scaled(width_+gap());
    return {int16_t(bounds_.x+inset),bounds_.y,int16_t(bounds_.width > inset ? bounds_.width-inset : 0),bounds_.height};
  }
  Rect focusBounds() const override {
    if (hasContent()) return bounds_;
    const Rect rect = indicatorBounds(); const int16_t edge = scale();
    return {int16_t(rect.x+edge),int16_t(rect.y+edge),int16_t(rect.width > edge*2 ? rect.width-edge*2 : 0),int16_t(rect.height > edge*2 ? rect.height-edge*2 : 0)};
  }
 protected:
  void drawContentBackground(Canvas& canvas,Color background,bool focused) const {
    if (hasContent()) canvas.fillRect(focusBounds(),focused ? inverted(background) : background);
  }
 private:
  uint8_t width_,height_;
};
}
