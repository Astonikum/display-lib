#include <MadDisplay/MadDisplay.hpp>
#include <MadSSD1306/MadSSD1306.hpp>

mad::SSD1306 adapter;
mad::Display<mad::SSD1306> display(adapter);
mad::Page pg;
mad::Label counter("0");
int value = 0;

// Also callable from timers, sensors or other application code.
void setValue(int next) {
  value = next;
  counter.setText(value);
}

void setup() {
  if (!display.begin(0x3C)) return;
  const int step = 1;
  pg.addRoot(
    mad::VerticalLayout().setGap(4).add(
      counter,
      mad::HorizontalLayout().add(
        mad::Button("+").onUse([step] { setValue(value+step); }),
        mad::Button("Reset").onUse([] { setValue(0); })
      )
    )
  );
  display.show(pg);
}

void loop() {
  // Map physical input to display.dispatch(mad::Action::Use/Next/etc.).
  display.update();
}
