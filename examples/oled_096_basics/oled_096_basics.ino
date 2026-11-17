#include <MadDisplay.h>
#include <MadSSD1306.h>

mad::SSD1306 adapter;
mad::Display<mad::SSD1306> display(adapter);
mad::VerticalLayout page1Root;
mad::VerticalLayout page2Root;
mad::Page page1(page1Root);
mad::Page page2(page2Root);
mad::Compositor::PageId page1Id;
mad::Compositor::PageId page2Id;
mad::HorizontalLayout topLayout;
mad::Button button1("BUTTON 1");
mad::Button button2("BUTTON 2");
mad::HorizontalLayout secondLayout;
mad::Label label("LABEL");
mad::Button openPage2Button("PAGE 2");
mad::Label page2Label("Page2");

struct PageLink { mad::Compositor* compositor; mad::Compositor::PageId page; };
PageLink page2Link = {&display, mad::Compositor::kInvalidPage};

void openPage2(void* context) {
  PageLink* link = static_cast<PageLink*>(context);
  link->compositor->openPage(link->page);
}

void setup() {
  if (!display.begin()) while (true) {}  // Check I2C wiring/address (usually 0x3C).
  topLayout.setWidthMode(mad::SizeMode::Fit);
  secondLayout.setWidthMode(mad::SizeMode::Fit);
  topLayout.add(button1);
  topLayout.add(button2);
  secondLayout.add(label);
  secondLayout.add(openPage2Button);
  openPage2Button.onUse(openPage2, &page2Link);
  page1Root.add(topLayout);
  page1Root.add(secondLayout);
  page2Root.add(page2Label);
  page1Id = display.registerPage(page1);
  page2Id = display.registerPage(page2);
  page2Link.page = page2Id;
  display.openPage(page1Id);
  display.render();
}

void loop() {}
