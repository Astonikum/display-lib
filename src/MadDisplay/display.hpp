#pragma once
#if defined(ARDUINO)
#include <Arduino.h>
#endif
#include "core.hpp"

namespace mad {

struct RenderWork {
  uint32_t pixels = 0, spans = 0, points = 0;
};

// Optional counters observe the actual raster calls, without a second renderer.
class MeterCanvas final : public Canvas {
 public:
  MeterCanvas(Canvas& target,RenderWork& work) : target_(target),work_(work) { work_ = {}; }
  Size size() const override { return target_.size(); }
  PixelFormat format() const override { return target_.format(); }
  Rect viewport() const override { return target_.viewport(); }
  void clear(Color c = Color::Off) override { work_.pixels += uint32_t(size().width)*size().height; ++work_.spans; target_.clear(c); }
  void pixel(int16_t x,int16_t y,Color c) override { ++work_.points; target_.pixel(x,y,c); }
  void fixedPixel(int16_t x,int16_t y,Color c) override { ++work_.points; target_.fixedPixel(x,y,c); }
  void fillSpan(int16_t x,int16_t y,int16_t length,Color c) override {
    const int32_t left = x < 0 ? 0 : x, right = int32_t(x)+length < size().width ? int32_t(x)+length : size().width;
    if (y >= 0 && y < size().height && right > left) { work_.pixels += right-left; ++work_.spans; }
    target_.fillSpan(x,y,length,c);
  }
  void text(int16_t x,int16_t y,const char* text,uint8_t scale,Color c) override { drawDefaultText(*this,x,y,text,scale,c); }
  void present() override { target_.present(); }
 private:
  Canvas& target_; RenderWork& work_;
};

// Display owns all rendering; an adapter supplies storage and a transport.
// Keep the adapter alive for the entire lifetime of the display.
template <class Adapter>
class Display : public Compositor {
 public:
  explicit Display(Adapter& adapter) : adapter_(adapter) {}

  template <class... Args>
  bool begin(Args&&... args) { hasFrame_ = false; return adapter_.begin(static_cast<Args&&>(args)...); }

  void setColorProfile(const ColorProfile& profile) { profile_ = profile; }
  const ColorProfile& colorProfile() const { return profile_; }
  void render() const {
    if (profile_.mode == ColorMode::RGB) Compositor::render(adapter_);
    else { ColorProfileCanvas canvas(adapter_,profile_); Compositor::render(canvas); }
  }
  void render(uint32_t nowUs) const {
    const uint32_t budget = fps() == kUnlimitedFps ? 0 : 1000000UL/fps();
    if (hasFrame_ && uint32_t(nowUs-lastFrameUs_) < budget) return;
    hasFrame_ = true; lastFrameUs_ = nowUs;
    renderFrame(nowUs);
  }
  // Explicit frame boundary for hosts with an external scheduler (also useful on a board).
  void renderFrame(uint32_t nowUs,RenderWork* work = nullptr) const {
    if (work) { MeterCanvas meter(adapter_,*work); renderTarget(meter,nowUs); }
    else renderTarget(adapter_,nowUs);
  }
  void update(uint32_t nowUs) const { render(nowUs); }
#if defined(ARDUINO)
  void update() const { render(micros()); }
#else
  void update() const { render(); }
#endif
  Size size() const { return adapter_.size(); }

 private:
  void renderTarget(Canvas& target,uint32_t nowUs) const {
    if (profile_.mode == ColorMode::RGB) Compositor::render(target,nowUs);
    else { ColorProfileCanvas canvas(target,profile_); Compositor::render(canvas,nowUs); }
  }
  Adapter& adapter_;
  ColorProfile profile_;
  mutable uint32_t lastFrameUs_ = 0;
  mutable bool hasFrame_ = false;
};

}  // namespace mad
