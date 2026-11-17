#pragma once

#include <stdint.h>

namespace mad {

static constexpr uint16_t kDefaultFocusDurationMs = 144;

enum class Easing : uint8_t { Linear, CubicOut, CubicInOut, BackOut, BounceOut };

inline float ease(float t, Easing easing) {
  if (t <= 0) return 0;
  if (t >= 1) return 1;
  switch (easing) {
    case Easing::CubicOut: { const float u = 1 - t; return 1 - u * u * u; }
    case Easing::CubicInOut: {
      const float u = -2 * t + 2;
      return t < 0.5f ? 4 * t * t * t : 1 - u * u * u / 2;
    }
    case Easing::BackOut: {
      const float u = t - 1;
      return 1 + 2.70158f * u * u * u + 1.70158f * u * u;
    }
    case Easing::BounceOut:
      if (t < 1 / 2.75f) return 7.5625f * t * t;
      if (t < 2 / 2.75f) { t -= 1.5f / 2.75f; return 7.5625f * t * t + 0.75f; }
      if (t < 2.5f / 2.75f) { t -= 2.25f / 2.75f; return 7.5625f * t * t + 0.9375f; }
      t -= 2.625f / 2.75f; return 7.5625f * t * t + 0.984375f;
    default: return t;
  }
}

// Clock supplied by the caller; unsigned elapsed time survives millis() wrap.
class Tween {
 public:
  explicit Tween(float initial = 0) : from_(initial), to_(initial) {}
  void start(float from, float to, uint32_t nowMs, uint16_t durationMs,
             Easing easing = Easing::CubicOut) {
    from_ = from; to_ = to; started_ = nowMs; duration_ = durationMs; easing_ = easing;
  }
  void retarget(float to, uint32_t nowMs, uint16_t durationMs,
                Easing easing = Easing::CubicOut) {
    start(value(nowMs), to, nowMs, durationMs, easing);
  }
  bool active(uint32_t nowMs) const { return static_cast<uint32_t>(nowMs - started_) < duration_; }
  float value(uint32_t nowMs) const {
    const uint32_t elapsed = nowMs - started_;
    if (!duration_ || elapsed >= duration_) return to_;
    return from_ + (to_ - from_) * ease(static_cast<float>(elapsed) / duration_, easing_);
  }
 private:
  float from_, to_;
  uint32_t started_ = 0;
  uint16_t duration_ = 0;
  Easing easing_ = Easing::CubicOut;
};

}  // namespace mad
