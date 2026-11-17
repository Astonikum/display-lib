#pragma once

#include "core.hpp"

namespace mad {

enum class Entrance : uint8_t { Rise, Slide, Reveal };

// Wrap an existing component or layout; replay() restarts its entrance.
class Motion final : public Component {
 public:
  explicit Motion(Component& child, Entrance effect = Entrance::Rise,
                  uint16_t durationMs = 480, uint16_t delayMs = 0)
      : child_(child), effect_(effect), durationMs_(durationMs), delayMs_(delayMs) {}
  Size preferredSize() const override { return child_.intrinsicSize(); }
  void replay() { pending_ = true; }
  void setEasing(Easing easing) { easing_ = easing; }
  void update(uint32_t nowMs) override {
    if (pending_) { started_ = nowMs; pending_ = false; }
    const uint32_t elapsed = nowMs - started_;
    progress_ = elapsed < delayMs_ ? 0 : (!durationMs_ || elapsed - delayMs_ >= durationMs_ ? 1
        : ease(static_cast<float>(elapsed - delayMs_) / durationMs_, easing_));
    child_.update(nowMs);
  }
  void draw(Canvas& canvas) const override {
    if (!visible_) return;
    Rect clip = bounds_;
    int16_t dx = 0, dy = 0;
    if (effect_ == Entrance::Reveal) clip.width = static_cast<int16_t>(clip.width * (progress_ < 0 ? 0 : (progress_ > 1 ? 1 : progress_)));
    else if (effect_ == Entrance::Slide) dx = static_cast<int16_t>(bounds_.width * (1 - progress_) + 0.5f);
    else dy = static_cast<int16_t>(bounds_.height * (1 - progress_) + 0.5f);
    CanvasView view(canvas, clip, dx, dy);
    child_.draw(view);
  }
  Rect contentExtent() const override { return visible_ ? child_.contentExtent() : Rect{0,0,0,0}; }
  bool scrollAt(Point position, Point delta) override {
    if (!visible_ || !containsPoint(bounds_,position)) return false;
    if (effect_ == Entrance::Reveal && position.x >= bounds_.x+bounds_.width*progress_) return false;
    if (effect_ == Entrance::Slide) position.x -= static_cast<int16_t>(bounds_.width*(1-progress_)+.5f);
    if (effect_ == Entrance::Rise) position.y -= static_cast<int16_t>(bounds_.height*(1-progress_)+.5f);
    return child_.scrollAt(position,delta);
  }
  void setScrollAnimation(uint16_t durationMs) override { child_.setScrollAnimation(durationMs); }
  bool focusClip(const Component& target, Rect& clip) const override {
    if (this == &target) return true;
    const int16_t dx = effect_ == Entrance::Slide ? int16_t(bounds_.width*(1-progress_)+.5f) : 0;
    const int16_t dy = effect_ == Entrance::Rise ? int16_t(bounds_.height*(1-progress_)+.5f) : 0;
    clip.x -= dx; clip.y -= dy;
    const bool found = child_.focusClip(target,clip);
    clip.x += dx; clip.y += dy;
    Rect visible = bounds_;
    if (effect_ == Entrance::Reveal) visible.width = int16_t(visible.width*(progress_ < 0 ? 0 : progress_ > 1 ? 1 : progress_));
    if (found) clip = intersectBounds(clip,visible);
    return found;
  }
  bool locate(const Component& target, Rect& result, bool destination = false) const override {
    if (Component::locate(target,result,destination)) return true;
    if (!child_.locate(target,result,destination)) return false;
    if (!destination) {
      if (effect_ == Entrance::Slide) result.x += static_cast<int16_t>(bounds_.width * (1 - progress_) + .5f);
      if (effect_ == Entrance::Rise) result.y += static_cast<int16_t>(bounds_.height * (1 - progress_) + .5f);
    }
    return true;
  }
  void collectFocusables(Component** result, uint8_t& count, uint8_t capacity) override {
    if (visible_ && enabled_) child_.collectFocusables(result, count, capacity);
  }
  bool ensureVisible(const Component& target) override {
    return &target == &child_ || child_.ensureVisible(target);
  }
  void layout(Rect parent) override { Component::layout(parent); layoutChildren(); }
 protected:
  void layoutChildren() override { child_.layout(bounds_); }
 private:
  Component& child_;
  Entrance effect_;
  Easing easing_ = Easing::CubicOut;
  uint16_t durationMs_, delayMs_;
  uint32_t started_ = 0;
  float progress_ = 1;
  bool pending_ = true;
};

}  // namespace mad
