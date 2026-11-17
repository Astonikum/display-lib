#pragma once
#include <stdint.h>
namespace mad {
inline uint32_t nextUtf8Codepoint(const char*& text) {
  if (!text || !*text) return 0;
  const uint8_t first = static_cast<uint8_t>(*text++);
  if (first < 0x80) return first;
  uint8_t count = 0; uint32_t value = 0,minimum = 0;
  if (first >= 0xc2 && first <= 0xdf) { count = 1; value = first&31; minimum = 0x80; }
  else if (first >= 0xe0 && first <= 0xef) { count = 2; value = first&15; minimum = 0x800; }
  else if (first >= 0xf0 && first <= 0xf4) { count = 3; value = first&7; minimum = 0x10000; }
  else return 0xfffd;
  for (uint8_t i = 0; i < count; ++i) {
    const uint8_t next = static_cast<uint8_t>(*text);
    if (!next || (next&0xc0) != 0x80) return 0xfffd;
    value = (value<<6)|(next&63); ++text;
  }
  return value < minimum || value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff) ? 0xfffd : value;
}

}
