#include "../examples/animation_showcase/showcase.hpp"
#include <cassert>
#include <cstring>

template<int16_t W, int16_t H> void checkShowcase() {
  mad_demo::Showcase demo;
  mad::BareBuffer<W, H> canvas;
  uint8_t previous[canvas.kByteCount];
  uint32_t hash = 0;
  unsigned changed = 0;
  for (uint32_t frame = 0; frame < 1800; ++frame) {
    std::memcpy(previous, canvas.pixels(), canvas.byteCount());
    demo.render(canvas, 0xffff0000u + frame * 16667u);
    if (std::memcmp(previous, canvas.pixels(), canvas.byteCount())) ++changed;
    for (size_t i = 0; i < canvas.byteCount(); ++i) hash = hash * 31 + canvas.pixels()[i];
  }
  assert(changed > 300 && hash != 0);
  for (uint32_t i = 1800; i < 1850; ++i) { demo.next(); demo.render(canvas, 0xffff0000u + i * 16667u, false); }
  assert(demo.compositor.currentPageId() != mad::Compositor::kInvalidPage);
}
int main() {
  checkShowcase<128,64>();
  checkShowcase<64,48>();
  checkShowcase<128,32>();
}
