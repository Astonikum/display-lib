#include <MadDisplay/MadDisplay.hpp>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <algorithm>

// Reference path used before span forwarding: each clipped pixel crosses every view.
class PixelView final : public mad::Canvas {
 public:
  explicit PixelView(mad::Canvas& target) : target_(target), dimensions_(target.size()) {}
  mad::Size size() const override { return target_.size(); }
  void clear(mad::Color color = mad::Color::Off) override { const auto s = size(); fillRect({0,0,s.width,s.height},color); }
  void pixel(int16_t x,int16_t y,mad::Color color) override {
    if (x >= 0 && y >= 0 && x < dimensions_.width && y < dimensions_.height) target_.pixel(x,y,color);
  }
  void text(int16_t x,int16_t y,const char* text,uint8_t scale,mad::Color color) override { mad::drawDefaultText(*this,x,y,text,scale,color); }
  void present() override {}
 private:
  mad::Canvas& target_;
  mad::Size dimensions_;
};
static volatile uint32_t checksum = 0;
static double measure(mad::Canvas& view,mad::RGB565Buffer<240,320>& buffer) {
  double times[5];
  for (int run = 0; run < 5; ++run) {
    const auto start = std::chrono::steady_clock::now();
    for (int frame = 0; frame < 100; ++frame) {
      view.fillRect({0,0,240,320},frame%2 ? mad::Color::Blue : mad::Color::Red);
      checksum += buffer.rgbPixels()[(frame*719)%76800];
    }
    times[run] = std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
  }
  std::sort(times,times+5); return times[2];
}
int main() {
  mad::RGB565Buffer<240,320> reference,current;
  PixelView a(reference),b(a),c(b);
  mad::CanvasView x(current,{0,0,240,320}),y(x,{0,0,240,320}),z(y,{0,0,240,320});
  const double perPixel = measure(c,reference),spans = measure(z,current);
  if (std::memcmp(reference.pixels(),current.pixels(),current.byteCount()) != 0) return 1;
  std::printf("{\"case\":\"100 RGB565 fills through 3 views\",\"reference_ms\":%.3f,\"spans_ms\":%.3f,\"ratio\":%.2f}\n",perPixel,spans,spans > 0 ? perPixel/spans : 0);
}
