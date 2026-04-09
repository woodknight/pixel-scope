# Architecture

## Modules
- core/
- io/
- raw/
- render/
- ui/

## Flow
ImageData → RAW pipeline → Renderer → Screen

## Phase 1 foundation

- `core/`
  Holds the testable truth-path pieces: image storage, histogram computation, and viewport math.
- `io/`
  Owns file dialogs and PNG/JPEG decode.
- `render/`
  Uploads immutable RGBA8 image data into an SDL texture with nearest-neighbor sampling.
- `ui/`
  Runs the SDL2 + Dear ImGui shell, canvas interaction, menus, and status bar.

## Current data split

- Source data:
  `core::ImageData` stores the loaded RGBA8 buffer plus source metadata.
- Image model:
  `core::ImageModel` keeps the full-resolution source image plus a few nearest-neighbor downsampled display levels. The UI picks the smallest display level that still maps to roughly one screen pixel per texel, which avoids an expensive full-resolution texture upload during the initial fit-to-window view of very large images.
- Derived inspection data:
  `core::ImageHistogram` is computed from `core::ImageData` and cached by the app so UI drawing does not re-scan pixels every frame.
- View state:
  `core::ViewState` stores zoom and pan only.
- Display data:
  `render::TextureCache` mirrors the currently selected display level into an SDL texture for fast drawing, while pixel inspection still reads from the full-resolution `core::ImageData`.

## Follow-up tasks

- Extend the histogram model to support log scale, clipping indicators, and view-vs-full-image modes.
- Consider moving image decoding and preview generation off the UI thread so opening very large images does not stall input handling.
- Add TIFF support behind the same `io/` interface.
- Persist recent files and last window state.
- Separate fit-to-window state from manual zoom state a bit more explicitly.
