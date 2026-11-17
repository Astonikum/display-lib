#include <cassert>
#define max(a,b) ((a) > (b) ? (a) : (b))
#define min(a,b) ((a) < (b) ? (a) : (b))
#include <MadDisplay/MadDisplay.hpp>
int main() {
  const auto frame = mad::Frame::fill(); const auto size = mad::SizeSpec::fill();
  assert(frame.width.unit == mad::Unit::Max && size.height.unit == mad::Unit::Max);
  assert((mad::Frame::max)().width.unit == mad::Unit::Max);
  assert(max(2,3) == 3 && min(2,3) == 2);
}
