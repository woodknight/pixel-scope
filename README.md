# PixelScope

PixelScope is a desktop image inspection tool for exact pixel work. It is built for cases where you need to zoom deeply, pan quickly, inspect individual pixels, and verify what the image data is actually doing without interpolation or UI noise getting in the way.

The current application is a lightweight SDL2 + Dear ImGui desktop viewer with a CPU truth-path for inspection and a nearest-neighbor display path for rendering.

## Features

- Open images from the menu, with `Ctrl+O`, by drag and drop, or from the command line.
- Inspect PNG, JPEG, and DNG files.
- Zoom with the mouse wheel and pan by dragging.
- Use nearest-neighbor display scaling so source pixels stay crisp.
- Read exact pixel values from the status bar while hovering.
- View raw Bayer DNG data as grayscale with raw sample readout.
- Toggle a histogram overlay from the `View` menu.
- Toggle a pixel grid overlay at high zoom levels.
- Reset to fit-to-window or 1:1 zoom from the `View` menu.
- Use cached display levels for large images so fit-to-window views stay responsive.

## How It Works

PixelScope keeps a full-resolution source image in memory for inspection, then chooses a smaller precomputed display level when zoomed out. That means pixel readout still comes from the original image data, while drawing can stay fast for large files.

The current UI also shows the active SDL renderer in the status bar. If it says `software`, interaction will usually feel much slower than on a machine using GPU acceleration.

## Build Requirements

You need these tools installed before configuring the project:

- CMake 3.24 or newer
- A C++20 compiler
- Rust toolchain with `cargo` and `rustc`
- `libtiff` development files
- Git
- Network access during the first CMake configure so dependencies can be fetched automatically

The project fetches these libraries during configure:

- SDL2
- Dear ImGui
- `stb`
- `tinyfiledialogs`

## Linux Dependencies

On Ubuntu or Debian, this is a good starting point:

```bash
sudo apt update
sudo apt install -y \
  build-essential \
  cmake \
  git \
  pkg-config \
  libtiff-dev \
  curl
```

Install Rust if it is not already available:

```bash
curl https://sh.rustup.rs -sSf | sh
source "$HOME/.cargo/env"
```

For smooth hardware-accelerated rendering on Linux, install the graphics userspace packages too:

```bash
sudo apt install -y \
  libgl1-mesa-dev \
  libegl1-mesa-dev \
  libgles2-mesa-dev \
  mesa-utils
```

If your machine uses NVIDIA, make sure the proper proprietary driver is installed and active. If PixelScope reports `Renderer software` in the status bar, SDL was not able to get an accelerated backend.

## Compile

Configure the build:

```bash
cmake -S . -B build
```

Build the application:

```bash
cmake --build build -j
```

The executable will be created at:

```bash
./build/pixelscope
```

## Run

Start the app with no file:

```bash
./build/pixelscope
```

Open an image immediately by passing a path:

```bash
./build/pixelscope /path/to/image.png
```

You can also open files after launch from:

- `File -> Open...`
- `Ctrl+O`
- drag and drop onto the window

## Usage

Once an image is open:

- Scroll the mouse wheel to zoom in and out.
- Drag with the left mouse button to pan.
- Hover over the image to inspect the current pixel in the status bar.
- Use `View -> Fit to Window` to reset the image to the canvas.
- Use `View -> 1:1 Zoom` to inspect pixels at native scale.
- Use `View -> Histogram Overlay` to show or hide the histogram.
- Use `View -> Pixel Grid` to show a grid when zoomed in far enough.
- For Bayer DNG images, use `View -> DNG CFA Colors` to switch between grayscale raw-plane display and CFA-colored display.

## Tests

Build and run the test target with:

```bash
cmake --build build --target pixelscope_tests
ctest --test-dir build --output-on-failure
```

Some tests expect image fixtures to be present in the repository checkout. If those files are missing, the test executable will fail even if the application itself builds correctly.

## Notes

- Pixel inspection reads from the source image model, not from the display texture.
- Display scaling is nearest-neighbor only.
- Histogram data is computed from the loaded image and cached for reuse.
- DNG support currently focuses on inspection and display, not full RAW workflow controls.
- TIFF support exists in the codebase as an intended format path, but the current README only documents formats that are already part of the active application flow.
