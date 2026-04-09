# PixelScope

PixelScope is a minimal desktop image inspection tool for exact pixel work.

This Phase 1 foundation includes:
- a cross-platform SDL2 + Dear ImGui desktop shell
- a main canvas window with nearest-neighbor zoom and pan
- PNG and JPEG loading
- exact CPU-side pixel readout in the status bar
- a small testable `core/` layer for image data and viewport math

## Build

Requirements:
- CMake 3.24+
- a C++20 compiler
- network access during the first configure so CMake can fetch SDL2, Dear ImGui, `stb`, and `tinyfiledialogs`

Build and run:

```bash
cmake -S . -B build
cmake --build build
./build/pixelscope
```

Run tests:

```bash
ctest --test-dir build --output-on-failure
```

Open an image from the `File` menu, with `Ctrl+O`, or by passing a file path on the command line:

```bash
./build/pixelscope /path/to/image.png
```

## Notes

- Display uses nearest-neighbor scaling only.
- Pixel inspection reads from the CPU image model, not the display texture.
- RAW, TIFF, histogram, and metadata panels are still follow-up work.
