# Rendering and transfer checks

Host measurement, macOS / Clang -O2, 2026-10-29. This is an isolated fill
benchmark, not an embedded FPS claim: 100 RGB565 fills at 240×320 through three
nested clipped views, median of five runs, identical final pixels.

| Path | Time per 100 fills |
| --- | ---: |
| Reference per-pixel forwarding | 25.922 ms |
| Scanline forwarding | 0.385 ms |
| Ratio in this case | 67.40× |

Reproduce:

```sh
cmake -S display-studio -B /tmp/mad-studio-build
cmake --build /tmp/mad-studio-build --target maddisplay_benchmark
/tmp/mad-studio-build/maddisplay_benchmark
```

Deterministic controller-emulator checks (payload counts exclude bus command
and transaction overhead):

- ILI9341 240×320 full frame: 76,800 pixels / 153,600 bytes.
- ILI9341 unchanged frame: no SPI transaction.
- ILI9341 single changed/erased pixel: 1 pixel / 2 bytes.
- ILI9341 changed 10×5 rectangle: 50 pixels in 5 row windows.
- SSD1306 128×64 full frame: 1,024 page data bytes.
- SSD1306 unchanged frame: no I²C transaction.
- SSD1306 single changed pixel: 1 page byte plus window/control overhead.
- SSD1306 failed I²C update: full retry restores the controller image.

RGB and bicolor Studio OpenGL smoke tests each render 120 frames with no GL
errors. Preview geometry is one image quad instead of up to 76,800 pixel quads;
unchanged textures skip upload. Ellipses and focus spans are compared against
per-pixel reference rasters. The design model caches image fingerprints between
edits. High-level Display rendering honors the configured FPS cap.

The checked-in ILI9341 color example compiles for ESP32 against Adafruit ILI9341
1.7.0 / GFX 1.13.0. The bicolor OLED example compiles for Mega 2560 against
Adafruit SSD1306 2.5.17. Native tests cover packed palette buffers, source RGB24,
RGB565 conversion, color serialization, clipping, focus and all four rotations.
Physical SPI/I²C throughput, electrical timing, panel offsets and sustained frame
rate must still be measured on the actual devices. Differential ILI9341 updates
require a previous-frame buffer; see the adapter README for RAM/PSRAM sizing.
