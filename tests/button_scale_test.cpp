#include <MadDisplay/MadDisplay.hpp>
#include <cassert>
#include <initializer_list>
using namespace mad;

static void check(uint8_t scale,uint8_t padding,bool filled,bool focused,bool expanded) {
  RGB565Buffer<160,128> base, enlarged;
  base.clear(Color::Magenta); enlarged.clear(Color::Magenta);
  Button original("OK",ButtonStyle(Color::White,Color::Blue,1,padding,filled));
  Button scaled("OK",ButtonStyle(Color::White,Color::Blue,scale,padding,filled));
  const Size normal = original.preferredSize(), larger = scaled.preferredSize();
  assert(larger.width == normal.width*scale && larger.height == normal.height*scale);
  const int16_t w = normal.width+(expanded ? 8 : 0), h = normal.height+(expanded ? 6 : 0);
  original.setSizeMode(SizeMode::Max,SizeMode::Max); scaled.setSizeMode(SizeMode::Max,SizeMode::Max);
  original.layout({5,5,w,h}); scaled.layout({5,5,int16_t(w*scale),int16_t(h*scale)});
  original.setFocused(focused); scaled.setFocused(focused);
  original.draw(base); scaled.draw(enlarged);
  const Rect focus = scaled.focusBounds();
  assert(focus.x == 5+scale && focus.y == 5+scale && focus.width == (w-2)*scale && focus.height == (h-2)*scale);
  // Every base pixel, including cut corners, padding and fixed borders, expands equally.
  for (int y = -1; y <= h; ++y) for (int x = -1; x <= w; ++x)
    for (int dy = 0; dy < scale; ++dy) for (int dx = 0; dx < scale; ++dx)
      assert(base.pixel(5+x,5+y) == enlarged.pixel(5+x*scale+dx,5+y*scale+dy));
}
int main() {
  for (uint8_t scale = 1; scale <= 3; ++scale) for (uint8_t padding = 0; padding <= 2; ++padding)
    for (bool filled : {false,true}) for (bool focused : {false,true}) for (bool expanded : {false,true})
      check(scale,padding,filled,focused,expanded);
  Button fallback("OK",ButtonStyle(Color::On,Color::Off,0)), normal("OK");
  assert(fallback.preferredSize().width == normal.preferredSize().width);
  // A constrained allocation must not paint scaled corners outside its bounds.
  RGB565Buffer<16,16> small; small.clear(Color::Magenta);
  Button clipped("",ButtonStyle(Color::White,Color::Blue,3,1,true));
  clipped.setSizeMode(SizeMode::Max,SizeMode::Max);
  clipped.layout({5,5,2,2}); clipped.draw(small);
  assert(small.pixel(4,5) == Color::Magenta && small.pixel(7,5) == Color::Magenta);
}
