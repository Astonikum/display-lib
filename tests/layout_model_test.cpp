#include <MadDisplay/MadDisplay.hpp>
#include <cassert>
using namespace mad;

struct Tile : Component {
  Size preferredSize() const override { return {8,6}; }
  void draw(Canvas& canvas) const override { canvas.fillRect(bounds_,Color::On); }
};
struct CountingCanvas : Canvas {
  Size size() const override { return {100,100}; }
  void clear(Color) override {}
  void pixel(int16_t,int16_t,Color) override { ++pixels; }
  void fillRect(Rect rect,Color) override { ++fills; last = rect; }
  void text(int16_t,int16_t,const char*,uint8_t,Color) override {}
  void present() override {}
  int pixels = 0, fills = 0; Rect last{};
};
static void paint(Canvas& canvas, Size size, uint32_t, void*) {
  assert(canvas.size().width == size.width && canvas.size().height == size.height);
  canvas.clear(Color::On);
}
int main() {
  VerticalLayout defaultRoot; Page defaultPage(defaultRoot); BareBuffer<64,64> rootScreen;
  defaultPage.draw(rootScreen,{2,3,40,30});
  assert(defaultRoot.bounds().x == 2 && defaultRoot.bounds().y == 3);
  assert(defaultRoot.bounds().width == 40 && defaultRoot.bounds().height == 30);
  defaultPage.draw(rootScreen,{0,0,64,64});
  assert(defaultRoot.bounds().width == 64 && defaultRoot.bounds().height == 64);
  defaultRoot.setWidth(Length::px(20)); defaultPage.draw(rootScreen);
  assert(defaultRoot.bounds().width == 20 && defaultRoot.bounds().height == 64);

  Tile tile;
  tile.setWidth(Length::px(20)); tile.layout({0,0,100,40});
  assert(tile.bounds().width == 20 && tile.bounds().height == 6);
  tile.setHeight(Length::percent(50)); tile.layout({0,0,100,40});
  assert(tile.bounds().height == 20);
  tile.setWidth(Length::autoSize()); tile.layout({0,0,50,80});
  assert(tile.bounds().width == 8 && tile.bounds().height == 40);
  tile.setHeight(Length::autoSize()); tile.layout({0,0,50,80});
  assert(tile.bounds().height == 6);
  tile.setSize(SizeSpec::px(-5,-10)); tile.layout({0,0,50,80});
  assert(tile.bounds().width == 0 && tile.bounds().height == 0);

  // Intrinsic parents include fixed-size descendants before the first draw.
  VerticalLayout root; Panel panel; GridLayout grid(1); Tile large;
  large.setSize(SizeSpec::px(40,30)); grid.add(large); panel.add(grid); root.add(panel);
  root.layout({0,0,10,10});
  assert(root.bounds().width == 44 && root.bounds().height == 34);
  assert(grid.bounds().width == 40 && large.bounds().width == 40);
  BareBuffer<64,64> screen; root.draw(screen); assert(screen.isOn(41,31));
  large.setWidth(Length::px(50)); root.layout({0,0,10,10});
  assert(root.bounds().width == 54 && large.bounds().width == 50);

  GridLayout autoGrid(1); Tile widePercent;
  widePercent.setWidth(Length::percent(200)); autoGrid.add(widePercent);
  autoGrid.layout({0,0,32,32}); screen.clear(); autoGrid.draw(screen);
  assert(autoGrid.bounds().width == 8 && widePercent.bounds().width == 16 && screen.isOn(15,0));

  // A small flow wrapper must not hide overflow from its scrolling ancestor.
  VerticalLayout scroll(Frame::px(0,0,32,20)); scroll.setScrollable(true); scroll.setGap(0);
  VerticalLayout wrapper(Frame::px(0,0,29,5)); wrapper.setGap(0);
  Button first("A"),last("B"); Tile spacer; spacer.setHeight(Length::px(30));
  wrapper.add(first); wrapper.add(spacer); wrapper.add(last); scroll.add(wrapper);
  scroll.setScrollAnimation(0); scroll.layout({0,0,32,20});
  assert(scroll.scrollContentExtent() == 52);
  assert(scroll.ensureVisible(last));
  assert(scroll.scrollOffset() == 32 && last.bounds().y+last.bounds().height == 20);
  screen.clear(); scroll.draw(screen); assert(screen.isOn(0,10));
  spacer.setVisible(false); scroll.layout({0,0,32,20});
  assert(scroll.scrollContentExtent() == 22 && scroll.scrollOffset() == 2);

  // The focus body and its animated trail stay inside an offset viewport.
  VerticalLayout pageRoot; FreeLayout board(Frame::px(0,0,48,48)); pageRoot.add(board);
  VerticalLayout inner(Frame::px(5,7,30,15)); inner.setScrollable(true);
  Button a("A"),b("B"),c("C"); inner.add(a); inner.add(b); inner.add(c); board.add(inner);
  Page page(pageRoot); Compositor display; display.setSafeArea(Insets(0));
  display.setNavigationAnimation(144,100); display.openPage(display.registerPage(page));
  display.render(screen,0); display.render(screen,200000);
  assert(page.focus(c));
  for (uint32_t t = 200000; t <= 500000; t += 10000) {
    display.render(screen,t);
    for (int y = 0; y < 48; ++y) for (int x = 0; x < 48; ++x)
      if (x < 5 || x >= 35 || y < 7 || y >= 22) assert(!screen.isOn(x,y));
  }
  assert(inner.scrollOffset() == 20 && page.scrollPosition().y == 0);

  // Reveal proceeds from the inner viewport to the outer one.
  VerticalLayout nestedRoot, outerScroll(Frame::px(0,0,32,20)); outerScroll.setScrollable(true);
  VerticalLayout innerScroll(Frame::px(0,0,29,15)); innerScroll.setScrollable(true);
  Tile before; before.setHeight(Length::px(30)); Button one("1"),two("2"),three("3");
  innerScroll.add(one); innerScroll.add(two); innerScroll.add(three);
  outerScroll.add(before); outerScroll.add(innerScroll); nestedRoot.add(outerScroll);
  Page nestedPage(nestedRoot); nestedPage.setNavigationAnimation(0,0); nestedPage.draw(screen);
  assert(nestedPage.focus(three)); nestedPage.draw(screen);
  assert(innerScroll.scrollOffset() == 20 && outerScroll.scrollOffset() == 26);
  assert(three.bounds().y >= 0 && three.bounds().y+three.bounds().height <= 20);

  // Nested transformed views expose the effective clip and keep bulk fills.
  CountingCanvas target; CanvasView outer(target,{10,10,20,20},5,5);
  CanvasView nested(outer,{0,0,40,40},2,3);
  const Rect clip = nested.viewport();
  assert(clip.x == 3 && clip.y == 2 && clip.width == 20 && clip.height == 20);
  nested.fillRect({-100,-100,200,200},Color::On);
  assert(target.fills == 1 && target.pixels == 0);
  assert(target.last.x == 10 && target.last.y == 10 && target.last.width == 20 && target.last.height == 20);
  BareBuffer<32,32> pixels; CanvasView offscreen(pixels,{0,0,8,8});
  CanvasComponent custom({20,20},paint); custom.setFrame(Frame::px(-5,-5,20,20)); custom.layout({0,0,32,32}); custom.draw(offscreen);
  assert(pixels.isOn(0,0) && pixels.isOn(7,7) && !pixels.isOn(8,7));
}
