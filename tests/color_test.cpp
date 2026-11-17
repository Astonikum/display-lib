#include <MadDisplay/MadDisplay.hpp>
#include <cassert>
#include <cstring>
#include <initializer_list>
using namespace mad;

static void ellipseReference(Canvas& canvas,Rect r,bool filled) {
  const int64_t ww = int64_t(r.width)*r.width, hh = int64_t(r.height)*r.height;
  const auto inside = [&](int x,int y) {
    if (x < 0 || y < 0 || x >= r.width || y >= r.height) return false;
    const int64_t dx = 2*x-r.width+1,dy = 2*y-r.height+1;
    return dx*dx*hh+dy*dy*ww <= ww*hh;
  };
  for (int y = 0; y < r.height; ++y) for (int x = 0; x < r.width; ++x)
    if (inside(x,y) && (filled || !inside(x-1,y) || !inside(x+1,y) || !inside(x,y-1) || !inside(x,y+1))) canvas.pixel(r.x+x,r.y+y,Color::Cyan);
}
struct SpanCounter : RGB565Buffer<240,320> {
  unsigned pixelsWritten = 0, spans = 0;
  void pixel(int16_t x,int16_t y,Color color) override { ++pixelsWritten; RGB565Surface::pixel(x,y,color); }
  void fillSpan(int16_t x,int16_t y,int16_t length,Color color) override { ++spans; RGB565Surface::fillSpan(x,y,length,color); }
};
struct CountedTarget : RGB565Buffer<32,32> {
  unsigned presentations = 0;
  bool begin() { return true; }
  void present() override { ++presentations; }
};
template<class Control>
static void compareColoredFocus(Control& native,Control& animated) {
  VerticalLayout nativeRoot,animatedRoot; nativeRoot.add(native); animatedRoot.add(animated);
  Page nativePage(nativeRoot),animatedPage(animatedRoot);
  RGB565Buffer<128,64> expected,actual; nativePage.draw(expected,{1,1,126,62});
  Compositor display; display.openPage(display.registerPage(animatedPage)); display.render(actual,0); display.render(actual,500000);
  assert(std::memcmp(expected.pixels(),actual.pixels(),actual.byteCount()) == 0);
}
int main() {
  static_assert(sizeof(Color) == 4,"Source colors retain all 24 RGB bits");
  assert(rgb(255,0,0) == Color::Red && rgb(0,255,0) == Color::Green && rgb(0,0,255) == Color::Blue);
  assert(inverted(Color::Blue) == Color::Yellow && inverted(Color::On) == Color::Off);
  for (unsigned value = 0; value < 65536; ++value) {
    const auto color = fromRGB565(value); assert(rgb565(color) == value);
  }
  RGB565Buffer<9,7> rgbBuffer; rgbBuffer.clear(Color::Blue);
  rgbBuffer.fillRect({-2,2,7,2},Color::Red);
  assert(rgbBuffer.pixel(0,2) == Color::Red && rgbBuffer.pixel(5,2) == Color::Blue);
  assert(rgbBuffer.format() == PixelFormat::RGB565 && rgbBuffer.byteCount() == 126);
  assert(!rgbBuffer.setStorage({32767,32767},nullptr,0));
  BareBuffer<9,7> mono; mono.clear(Color::Red); assert(!mono.isOn(0,0)); mono.fillSpan(1,1,4,Color::Yellow); assert(mono.isOn(1,1));
  const Palette palette(Color::Black,Color::Red,Color::White,Color::Yellow,3);
  PaletteBuffer<9,7> indexed(palette); indexed.clear(Color::Red);
  indexed.fillSpan(-2,1,7,Color::White); indexed.pixel(8,6,Color::Black);
  assert(indexed.format() == PixelFormat::Indexed2 && indexed.byteCount() == 16);
  for (int y = 0; y < 7; ++y) for (int x = 0; x < 9; ++x)
    assert(indexed.pixel(x,y) == (x == 8 && y == 6 ? Color::Black : y == 1 && x < 5 ? Color::White : Color::Red));
  TwoColorBuffer<9,7> bicolor(Palette(Color::Blue,Color::Yellow)); bicolor.clear(Color::Blue); bicolor.fillSpan(2,3,4,Color::Yellow);
  assert(bicolor.byteCount() == 8 && bicolor.format() == PixelFormat::Indexed1);
  assert(bicolor.pixel(2,3) == Color::Yellow && bicolor.pixel(6,3) == Color::Blue);
  ColorRegion regions[] = {{{0,0,5,2},Color::Yellow},{{0,2,5,5},Color::Blue},{{5,0,4,7},Color::Red}};
  ColorProfile profile; profile.mode = ColorMode::Zoned; profile.regions = regions; profile.regionCount = 3;
  assert(profile.map(Color::White,0,0) == Color::Yellow && profile.map(Color::White,0,3) == Color::Blue);
  assert(profile.map(Color::White,6,3) == Color::Red && profile.map(Color::Black,6,3) == Color::Black);
  RGB565Buffer<9,7> projected; ColorProfileCanvas projection(projected,profile); projection.clear(Color::White);
  assert(projected.pixel(0,0) == Color::Yellow && projected.pixel(0,3) == Color::Blue && projected.pixel(6,3) == Color::Red);
  BareBuffer<9,7> logical; ColorProfileCanvas hardware(logical,profile); hardware.clear(Color::White);
  assert(logical.isOn(0,0) && logical.isOn(0,3)); // blue emitters must not turn logical white off
  profile.mode = ColorMode::Palette; profile.palette = palette; assert(profile.map(Color::Red,0,0) == Color::Red);

  RGB565Buffer<64,64> actual,expected;
  for (int w = 1; w <= 35; ++w) for (int h = 1; h <= 35; ++h) for (bool filled : {false,true}) {
    actual.clear(); expected.clear(); drawEllipse(actual,{3,4,static_cast<int16_t>(w),static_cast<int16_t>(h)},Color::Cyan,filled);
    ellipseReference(expected,{3,4,static_cast<int16_t>(w),static_cast<int16_t>(h)},filled);
    assert(std::memcmp(actual.pixels(),expected.pixels(),actual.byteCount()) == 0);
  }
  const Color colors[] = {Color::Red,Color::Green,Color::Blue,Color::Yellow}; const uint8_t coverage[] = {1,0,2,1};
  ImageComponent image(colors,{2,2},coverage); image.setFrame(Frame::px(1,1,4,4)); image.layout({0,0,64,64});
  actual.clear(Color::Magenta); image.draw(actual);
  assert(actual.pixel(1,1) == Color::Red && actual.pixel(4,1) == Color::Magenta && actual.pixel(1,4) == Color::Blue);
  LiquidFocus focus; focus.follow({0,0,32,32},true,0,0);
  LiquidFocusCanvas invertedCanvas(actual,focus,{0,0}); invertedCanvas.fillSpan(0,0,60,Color::Red);
  assert(actual.pixel(10,0) == inverted(Color::Red) && actual.pixel(50,0) == Color::Red);
  LiquidFocus moving;
  moving.follow({2,3,15,11},true,0,144); moving.follow({39,34,21,17},true,1,144);
  for (uint32_t time = 1; time <= 170; time += 7) {
    moving.follow({39,34,21,17},false,time,144);
    for (Point camera : {Point{0,0},Point{-7,5}}) {
      actual.clear(); expected.clear(); LiquidFocusCanvas spans(actual,moving,camera),pixels(expected,moving,camera);
      for (int16_t y = 0; y < 64; ++y) {
        spans.fillSpan(-4,y,74,Color::Blue);
        for (int16_t x = -4; x < 70; ++x) pixels.pixel(x,y,Color::Blue);
      }
      assert(std::memcmp(actual.pixels(),expected.pixels(),actual.byteCount()) == 0);
    }
  }
  CountedTarget target; Display<CountedTarget> display(target); VerticalLayout root; Button button("A"); root.add(button); Page page(root);
  assert(display.begin()); display.setFps(60); assert(display.openPage(display.registerPage(page)));
  display.render(0); display.render(1000); assert(target.presentations == 1);
  display.render(16667); assert(target.presentations == 2);
  display.setFps(Compositor::kUnlimitedFps); display.render(16668); assert(target.presentations == 3);
  display.render(0xfffffff0u); display.setFps(60); display.render(100u); assert(target.presentations == 4);
  display.render(20000u); assert(target.presentations == 5);
  Button nativeButton("RGB",ButtonStyle(Color::Blue,Color::Black)),animatedButton("RGB",ButtonStyle(Color::Blue,Color::Black));
  compareColoredFocus(nativeButton,animatedButton);
  Checkbox nativeBox(false,CheckboxStyle(Color::Black,Color::Green)),animatedBox(false,CheckboxStyle(Color::Black,Color::Green));
  compareColoredFocus(nativeBox,animatedBox);
  RadioButton nativeRadio(true,RadioButtonStyle(Color::Black,Color::Magenta)),animatedRadio(true,RadioButtonStyle(Color::Black,Color::Magenta));
  compareColoredFocus(nativeRadio,animatedRadio);
  Switch nativeSwitch(false,SwitchStyle(Color::Blue,Color::Green,Color::White,Color::Yellow)),animatedSwitch(false,SwitchStyle(Color::Blue,Color::Green,Color::White,Color::Yellow));
  compareColoredFocus(nativeSwitch,animatedSwitch);
  SpanCounter counter; CanvasView view(counter,{0,0,240,320}); view.fillRect({0,0,240,320},Color::Blue);
  assert(counter.pixelsWritten == 0 && counter.spans == 320); // no per-pixel virtual dispatch for solid fills
}
