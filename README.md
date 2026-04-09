# PixelScope

PixelScope is a minimal desktop image inspection tool for exact pixel work.

This Phase 1 foundation includes:
- a cross-platform SDL2 + Dear ImGui desktop shell
- a main canvas window with nearest-neighbor zoom and pan
- PNG, JPEG, and initial DNG loading
- exact CPU-side pixel readout in the status bar
- a lower-left histogram overlay with a `View` menu toggle
- a small testable `core/` layer for image data, histogram computation, and viewport math

## Build

Requirements:
- CMake 3.24+
- a C++20 compiler
- network access during the first configure so CMake can fetch SDL2, Dear ImGui, `stb`, `tinydng`, and `tinyfiledialogs`

Configure and build:

```bash
cmake -S . -B build
cmake --build build --config Release
```

Run the app:

```bash
./build/pixelscope
```

Run tests:

```bash
cmake --build build --target pixelscope_tests
ctest --test-dir build --output-on-failure
```

Open an image from the `File` menu, with `Ctrl+O`, or by passing a file path on the command line:

```bash
./build/pixelscope /path/to/image.png
```

## Notes

- Display uses nearest-neighbor scaling only.
- Pixel inspection reads from the CPU image model, not the display texture.
- Large images now build a small set of nearest-neighbor preview levels in `core/` so the first on-screen upload can use a reduced display texture while keeping the full-resolution source image for inspection.
- Histogram computation is derived from the source `RGBA8` image model and rendered as a lightweight overlay in the UI.
- DNG support currently converts the largest decoded 8-bit or 16-bit payload into an RGBA8 display image. Single-channel Bayer payloads are shown as a grayscale raw plane without black/white normalization, and the status bar reports the untouched raw sample value.
- TIFF, richer RAW controls, and metadata panels are still follow-up work.
