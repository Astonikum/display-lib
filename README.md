# MadDisplay

Portable C++ display UI library. MadDisplay owns components, layout, colors,
focus, navigation, text rendering and motion. A separate adapter initializes
the physical display and presents rendered pixels. Studio uses the same UI
components and renderer as firmware.

## Starting a display

Install MadDisplay and the adapter for your controller, for example MadSSD1306
or MadILI9341. The adapter may require its own hardware driver dependency.

```cpp
#include <MadDisplay/MadDisplay.hpp>
#include <MadSSD1306/MadSSD1306.hpp>

mad::SSD1306 adapter;
mad::Display<mad::SSD1306> display(adapter);
mad::Page pg;
mad::Label counter("0");
int value = 0;

void setup() {
  if (!display.begin(0x3C)) return;
  pg.addRoot(
    mad::VerticalLayout().setGap(4).add(
      counter,
      mad::Button("+").onUse([] { counter.setText(++value); })
    )
  );
  display.show(pg);
}

void loop() {
  display.update();
}
```

Temporary roots and children belong to their page/tree. Named components such as
`counter` are borrowed, so changing `counter.setText(42)` outside `setup()` updates
the actual visible component on the next frame. Keep borrowed components, pages,
adapters, fonts, image pixels and `TextLayout` assets alive while used. Text strings
are copied into the component. Adapter-specific wiring and physical panel limits
are documented in each adapter package.

See [declarative_counter](examples/declarative_counter/declarative_counter.ino)
for a complete Arduino sketch, including a captured lambda.

## Trees, callbacks and updates

`addRoot()` accepts any component root, including horizontal, grid and free
layouts. The root defaults to 100% of the available area. `add(a,b,...)` accepts
both named components and temporaries; temporary layouts also support repeated
`.add(...)` calls. Common sizing/style setters preserve chaining and the concrete
component type. The existing `layout.add(namedChild)` still returns a success
boolean; the existing `Page(root)`, `registerPage()`, `openPage()` and `render()`
APIs remain supported.

`onUse()` accepts a no-argument callable, including `[this]`, `[&state]`, value
captures and mutable lambdas. `onClick()` is an alias. The original
`onUse(void (*)(void*), context)` overload remains available. Checkbox, Switch
and RadioButton update their state before invoking the handler; programmatic
setters do not invoke it. Captured references and `this` must outlive their use.

`Label::setText()` and `Button::setText()` accept C strings, Arduino `String`,
other values with `c_str()`, and numbers. Label also accepts an optional decimal count (`label.setText(23.5,1)`, up to six places).
Text is owned and copied, so a temporary String is safe. Reuse the setter after
changing an external character buffer; modifying that buffer alone no longer
changes the label. There is no automatic variable binding.

`display.show(pg)` registers and opens a persistent page. `display.update()` uses
Arduino `micros()` and the configured FPS limit; `update(nowUs)` supports an
external clock on any host. Property changes take effect on the next rendered
frame. `addRoot()` and `show()` return success booleans; fluent trees expose
`valid()`. The 16-child limit remains; an invalid fluent tree cannot replace a
valid page root.

Owned temporary components and callables use heap storage shared across
tree copies; named children remain borrowed. Labels own text buffers and reuse
their capacity across updates. The framework does not allocate these objects
again on each render. Build persistent trees in `setup()` rather than rebuilding
them in `loop()`.

## Components and composition

Controls include Label, Button, Switch, Checkbox and RadioButton. Control content
is a child component: use a Label, Image or a layout containing several children.
A content control has one content slot; a layout groups multiple children.
String constructors remain a convenience for existing applications.

Vertical and horizontal layouts, scrollable layouts, GridLayout, Panel and
FreeLayout arrange children. Scale applies to control content, border and padding.
Canvas, Image, Rectangle, Ellipse and Line provide graphics components.

The display handles directional focus, Previous/Next, Use and Back actions.
Motion includes entrance effects, page transitions, animated scrolling and an
elastic focus body. See the headers and examples for configuration APIs.

## Sizing and overflow

The page root defaults to 100% width and height of the available display area
(after safe-area insets). Explicit root dimensions remain configurable. Other
components default to content size (`Length::autoSize()`). Set either axis
independently; the next layout/render resolves the new value:

```cpp
button.setWidth(mad::Length::percent(80));
button.setHeight(mad::Length::px(24));
button.setHeight(mad::Length::autoSize());
```

Percentages use the immediate parent's content area (grid children use their
cell). `Length::fill()` and the existing `SizeMode::Max` remain available for
sharing remaining flow space. `SizeSpec` and `Frame` also accept mixed units;
`Frame` positions apply in `FreeLayout`. Pixel zero is an explicit zero size.
Auto containers include their children's explicit pixel sizes. Percentage and
fill dimensions contribute intrinsic content size when measuring an auto parent,
then resolve against its arranged bounds; they do not recursively enlarge it.

Ordinary flow containers allow visible overflow. Grid tracks constrain children
when the grid width is explicitly constrained. A scrollable layout clips content
and animated focus to its viewport, measures overflowing descendants and scrolls
to the focused control. Give it a fixed/percentage viewport, or use fill; enabling
scroll on an auto main axis defaults that axis to fill.

`CanvasComponent` draws in local coordinates. Its callback receives the resolved
size, time and context; `canvas.size()` matches that size and `canvas.viewport()`
reports the visible local area. Use the viewport to skip expensive offscreen
work. `setPreferredSize()` changes its intrinsic size and `setDrawHandler()`
replaces its drawing callback/context. Drawing is immediate, with no owned pixel
buffer. Nested views intersect their clips and forward bulk rectangle/span fills
to the target renderer.

## Text and colors

Text input uses UTF-8. The built-in small pixel font has limited glyph coverage;
custom BitmapFont tables can address any Unicode scalar. Prepared TextLayout
assets provide shaped multilingual text, combining marks and color emoji through
`Label::setTextLayout()`. The renderer is shared with Studio. See
[Unicode text](../display-studio/UNICODE.md) for preparing fonts, firmware
export, memory requirements and runtime limitations.

MadDisplay supports monochrome, palettes and RGB565. Fixed-color OLED emission
zones describe physical emitters; they do not turn a monochrome controller into
a freely addressable color panel. Adapters expose the capabilities of their
actual hardware.

## Studio and verification

[Mad Studio](../display-studio/README.md) designs and simulates display UIs.
Sketch compilation and device upload use external tools. The Studio CMake build
also runs portable library and adapter tests:

```sh
cmake -S display-studio -B /tmp/mad-studio-build
cmake --build /tmp/mad-studio-build -j
ctest --test-dir /tmp/mad-studio-build --output-on-failure
```

Physical performance and wiring require testing on the target board.

## Installation

Repository: https://github.com/Astonikum/display-lib

PlatformIO (`platformio.ini`):

```ini
lib_deps =
  https://github.com/Astonikum/display-lib.git#v0.7.0
```

The core has no hardware, vendor driver or adapter dependencies.

Arduino IDE: download **Code → Download ZIP**, then use **Sketch → Include Library → Add .ZIP Library**.

These packages are distributed through GitHub; they are not yet indexed in the Arduino or PlatformIO registries. Arduino IDE does not install a library directly from a Git URL.

Requires C++14 on the host. Embedded examples select their board and its Arduino toolchain in `platformio.ini`. RAM requirements are documented above; a matching controller does not guarantee a board has enough memory.

## Split from the old workspace

`display-lib/src` contains only UI, rasterization and framebuffer storage.
Controller profiles and transfer planners moved to `adapter-common-lib`.
`MadDisplay/oled096.hpp` is now supplied by `ssd1306-lib`. Existing namespaced
headers and adapter types remain available after installing their packages.
Core headers never include a vendor driver or a bus implementation.

Build core tests independently:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```
