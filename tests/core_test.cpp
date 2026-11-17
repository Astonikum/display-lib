#include <MadDisplay/MadDisplay.hpp>
#include <cassert>
#include <cmath>
#include <cstring>
#include <initializer_list>


struct Block : mad::Component {
  mad::Size preferredSize() const override { return {20, 10}; }
  void draw(mad::Canvas& c) const override { c.fillRect(bounds_, mad::Color::On); }
};
struct CountingBuffer : mad::BareBuffer<128, 64> {
  int presents = 0;
  void present() override { ++presents; }
};

int main() {
  using namespace mad;
  Tween t;
  t.start(0, 10, 0xfffffff0u, 100, Easing::CubicInOut);
  assert(t.value(0xfffffff0u) == 0);
  assert(std::fabs(t.value(34) - 5) < 0.001f);
  assert(t.value(84) == 10 && !t.active(84));
  t.start(3, 9, 0, 0); assert(t.value(0) == 9);
  for (Easing e : {Easing::Linear, Easing::CubicOut, Easing::CubicInOut, Easing::BackOut, Easing::BounceOut}) {
    assert(ease(0, e) == 0 && ease(1, e) == 1);
    for (int i = 0; i <= 100; ++i) assert(std::isfinite(ease(i / 100.f, e)));
  }
  t.start(0, 10, 0, 100, Easing::Linear);
  t.retarget(0, 50, 100); assert(t.value(50) == 5 && t.value(150) == 0);

  BareBuffer<13, 9> odd;
  odd.fillRect({-3, -2, 7, 5}, Color::On);
  assert(odd.isOn(3, 2) && !odd.isOn(4, 2) && !odd.isOn(-1, 0));
  odd.clear(); CanvasView clip(odd, {2, 2, 3, 3});
  clip.text(0, 0, "AAAA", 1, Color::On);
  for (int y = 0; y < 9; ++y) for (int x = 0; x < 13; ++x)
    if (x < 2 || x >= 5 || y < 2 || y >= 5) assert(!odd.isOn(x, y));

  VerticalLayout root;
  Block a, b, c;
  root.add(a); root.add(b); root.add(c);
  assert(!root.add(a) && !root.add(root));
  b.setVisible(false);
  assert(root.preferredSize().height == 21);
  root.layout({0, 0, 20, 15});
  assert(c.bounds().y == 11 && b.bounds().height == 0);
  root.setScrollable(true); root.layout({0, 0, 20, 15}); root.setScrollOffset(4);
  BareBuffer<32, 32> screen;
  root.draw(screen);
  assert(screen.isOn(0, 14) && !screen.isOn(0, 15));

  FreeLayout free(Frame::px(0, 0, 30, 20));
  Block positioned;
  free.add(positioned, Frame::px(3, 4, 10, 8));
  VerticalLayout freeRoot;
  freeRoot.add(free); freeRoot.layout({5, 6, 60, 40});
  assert(positioned.bounds().x == 8 && positioned.bounds().y == 10);

  VerticalLayout buttons;
  Button first("A"), second("B"), third("C");
  buttons.add(first); buttons.add(second); buttons.add(third);
  Page page(buttons);
  page.draw(screen); assert(page.focusedComponent() == &first);
  page.dispatch(Action::Down); assert(page.focusedComponent() == &second);
  first.setVisible(false); page.draw(screen); assert(page.focusedComponent() == &second);
  page.resetFocus(); assert(!second.focused());

  Switch sw;
  sw.layout({0, 0, 20, 11}); sw.update(0); sw.toggle(); sw.update(10); sw.update(120);
  BareBuffer<20, 11> mid, end;
  sw.draw(mid); sw.update(230); sw.draw(end);
  assert(std::memcmp(mid.pixels(), end.pixels(), mid.byteCount()) != 0);
  sw.toggle(); sw.update(240); sw.update(460);
  BareBuffer<20, 11> off, reference;
  sw.draw(off); Switch staticOff; staticOff.layout({0,0,20,11}); staticOff.draw(reference);
  assert(std::memcmp(off.pixels(), reference.pixels(), off.byteCount()) == 0);

  VerticalLayout oldRoot, newRoot;
  Block oldBlock; newRoot.add(sw); oldRoot.add(oldBlock);
  Page oldPage(oldRoot), newPage(newRoot, Color::On);
  Compositor compositor;
  auto oldId = compositor.registerPage(oldPage), newId = compositor.registerPage(newPage);
  compositor.setTransition(100); compositor.openPage(oldId);
  CountingBuffer output;
  compositor.render(output, 0xffff0000u);
  compositor.openPage(newId);
  compositor.render(output, 0xffff1000u);
  compositor.render(output, 0x00010000u);
  compositor.render(output, 0x00020000u);
  assert(output.presents == 4 && compositor.currentPageId() == newId);
  BareBuffer<128, 64> expected;
  newPage.draw(expected, {1, 1, 126, 62});
  assert(std::memcmp(output.pixels(), expected.pixels(), expected.byteCount()) == 0);
  assert(compositor.back()); compositor.render(output, 0x00030000u);
  compositor.render(output, 0x00050000u);
  assert(compositor.currentPageId() == oldId);

  Compositor metrics;
  metrics.sampleFrame(0); metrics.sampleFrame(4000000000u);
  assert(metrics.actualFrameTimeMs() == 65535);

  Block artwork; Motion motion(artwork, Entrance::Reveal, 100, 50);
  motion.layout({0, 0, 20, 10}); motion.update(0); screen.clear(); motion.draw(screen);
  assert(!screen.isOn(0, 0));
  motion.update(150); motion.draw(screen); assert(screen.isOn(19, 9));
  motion.replay(); motion.update(200); screen.clear(); motion.draw(screen); assert(!screen.isOn(0,0));
}
