#include <MadDisplay.h>
#include <MadSSD1306.h>
#include "showcase.hpp"

mad::SSD1306 oled;
mad_demo::Showcase showcase;

void setup() {
  if (!oled.begin()) while (true) {}
}
void loop() {
  const uint32_t started = micros();
  showcase.render(oled, started);
  const uint32_t elapsed = micros() - started;
  const uint32_t budget = 1000000UL / showcase.compositor.fps();
  if (elapsed < budget) delayMicroseconds(static_cast<unsigned int>(budget - elapsed));
}
