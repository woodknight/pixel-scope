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
- Derived inspection data:
  `core::ImageHistogram` is computed from `core::ImageData` and cached by the app so UI drawing does not re-scan pixels every frame.
- View state:
  `core::ViewState` stores zoom and pan only.
- Display data:
  `render::TextureCache` mirrors source data into an SDL texture for fast drawing, but pixel inspection still reads from `core::ImageData`.

## Follow-up tasks

- Extend the histogram model to support log scale, clipping indicators, and view-vs-full-image modes.
- Add TIFF support behind the same `io/` interface.
- Persist recent files and last window state.
- Separate fit-to-window state from manual zoom state a bit more explicitly.
