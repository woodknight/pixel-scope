# PixelScope — AI-agent-friendly product plan
## 1\. Product vision
**PixelScope** is a cross-platform desktop image inspection tool for engineers, researchers, and imaging developers.
It should feel like a **digital oscilloscope for pixels**:
-   instant open
-   instant zoom
-   exact pixel readout
-   no “pretty” image viewer behavior
-   engineered for truth, not decoration
Core principle: **show the image data faithfully and interactively, with as little friction as possible.**
---
## 2\. Product goals
### Primary goals
-   **Fast**: opens quickly, pans/zooms smoothly, low latency on large images
-   **Accurate**: pixel values must be trustworthy
-   **Minimal**: simple UI, few dependencies, clean architecture
-   **Cross-platform**: Linux, macOS, Windows
-   **Extensible**: easy to add formats and inspection tools later
### Non-goals for v1
-   Photo editing
-   Asset management / gallery / DAM
-   Layer compositing like Photoshop
-   Heavy color grading workflows
-   Cloud features / login / sync
---
## 3\. Target users
### Main users
-   imaging / ISP engineers
-   computer vision researchers
-   RAW pipeline developers
-   camera tuning / QA engineers
-   graphics / GPU developers
### Typical use cases
-   inspect exact pixel values
-   verify Bayer pattern / RAW unpacking
-   inspect histogram and clipping
-   validate DNG decoding and color rendering
-   inspect binary raw dumps quickly
-   compare rendering assumptions like AWB / CCM / demosaic
---
## 4\. Product pillars
### 4.1 Truth-first inspection
The app must preserve raw data semantics and never hide what is happening through silent smoothing, interpolation, or “beautification”.
### 4.2 Instant interaction
Zoom, pan, hover-readout, histogram toggle, and parameter changes should feel immediate.
### 4.3 Simple mental model
A user should understand the whole app in minutes:
-   open image
-   inspect
-   switch modes
-   tweak RAW parameters
-   read values
### 4.4 Lean architecture
Keep the codebase small, modular, and easy for agents or humans to extend.
---
## 5\. Functional requirements
## 5.1 File support
### Standard image formats
Support common formats:
-   PNG
-   JPEG
-   TIFF
-   BMP
-   WebP
Optional later:
-   EXR
-   JPEG XL
-   HEIF / HEIC
-   AVIF
### RAW formats
-   DNG
-   binary raw dumps
For DNG:
-   decode metadata
-   allow display as:
    -   rendered sRGB
    -   Bayer/raw plane view
For binary raw:
-   user specifies:
    -   width
    -   height
    -   bit depth
    -   packing format
    -   Bayer pattern
    -   endianness
    -   stride if needed
-   app can infer likely values from filename when possible
-   app remembers last-used settings per source or globally
---
## 5.2 Image viewing
### Zoom and pan
-   smooth mouse-wheel zoom
-   keyboard zoom shortcuts
-   drag to pan
-   fit-to-window
-   1:1 view
-   jump to exact zoom ratios
### Infinite zoom
-   allow very deep zoom-in
-   at high zoom, each source pixel should appear as a solid block
-   **no interpolation**
-   **no smoothing**
-   **no anti-aliasing**
-   optional grid overlay at high zoom levels
### Pixel inspection
When mouse hovers:
-   show x, y
-   show pixel value under cursor
-   for RGB images: R, G, B
-   for RAW/Bayer: raw sample value and CFA position
-   optional normalized value and integer value
-   optional hex display for packed data debugging
### Selection inspection
Nice v1.1 or v2 feature:
-   drag rectangular ROI
-   show:
    -   min / max
    -   mean
    -   stddev
    -   histogram for ROI only
---
## 5.3 Histogram
-   show histogram panel
-   toggle on/off instantly
-   support:
    -   luminance histogram
    -   per-channel histogram
    -   raw histogram
-   allow linear / log scale
-   show clipping indicators
-   optional histogram from current view or full image
Performance note:
-   histogram computation should be cached and incrementally updated when possible
---
## 5.4 DNG / RAW rendering controls
### DNG modes
Two main display modes:
1.  **Rendered mode**
    -   decode to displayable RGB
    -   output in sRGB
2.  **Raw mode**
    -   show Bayer/raw data directly
    -   visualize per-pixel sensor values faithfully
### Binary raw controls
Need a dedicated side panel with:
-   width
-   height
-   bit depth
-   packing
-   Bayer pattern
-   black level
-   white level
-   AWB gains
-   CCT slider or temperature/tint controls
-   CCM entry / preset
-   gamma / tone mapping toggle
-   channel view:
    -   RGB
    -   R only
    -   G only
    -   B only
    -   Bayer mosaic
    -   luminance
Important: clearly distinguish:
-   **data interpretation parameters**
-   **display-only parameters**
That keeps the tool honest.
---
## 6\. UX requirements
## 6.1 UI philosophy
-   minimalist
-   professional
-   low visual noise
-   modern but not flashy
-   keyboard-friendly
-   dark mode first, light mode optional later
### Layout suggestion
-   center: image canvas
-   top: lightweight toolbar
-   right: inspector panel
-   bottom/status bar:
    -   zoom
    -   cursor position
    -   pixel value
    -   image dimensions
    -   color mode / raw mode
### Panels
-   histogram panel: collapsible
-   raw controls panel: collapsible
-   metadata panel: optional collapsible
-   panels should remember last state
---
## 6.2 Interaction details
-   drag-and-drop open
-   recent files
-   reload file from disk
-   copy pixel value
-   copy current view info
-   keyboard shortcuts for common actions
-   remember window/layout state
---
## 7\. Performance requirements
### Startup
-   target near-instant cold start
-   avoid large framework overhead if possible
### Rendering
-   pan/zoom should stay responsive for large images
-   use tiled rendering for huge images
-   only process what is visible
-   cache pyramids or mip levels where useful, but never blur inspection mode
### GPU usage
GPU acceleration is worth considering for:
-   viewport rendering
-   zoom/pan compositing
-   tone mapping / RAW preview transforms
-   histogram acceleration for large images
But:
-   pixel readout correctness must never depend on lossy display path
-   keep a CPU-truth path for exact inspection
A good rule:
-   **GPU for display speed**
-   **CPU / precise buffer for truth**
---
## 8\. Engineering constraints
### Design constraints
-   clean code
-   modular architecture
-   least dependencies
-   no giant app framework unless clearly justified
-   easy for AI coding agents to work on safely
### Architecture principles
-   clear separation of concerns:
    -   file IO / decode
    -   image data model
    -   rendering
    -   UI
    -   RAW interpretation pipeline
-   immutable or well-controlled image state where possible
-   explicit typed data structures for image metadata and display params
-   plugin-like decoder interface for formats
---
## 9\. Proposed architecture
## 9.1 Core modules
### `core/`
Shared types and logic:
-   image model
-   pixel formats
-   metadata structs
-   viewport math
-   histogram engine
-   settings persistence
### `io/`
File loading and decoding:
-   standard image decoder adapters
-   DNG loader
-   binary raw loader
-   filename parser for raw dimensions / patterns
### `raw/`
RAW interpretation pipeline:
-   Bayer view
-   demosaic path
-   AWB
-   CCM
-   black/white level
-   gamma / display transform
### `render/`
Fast display engine:
-   CPU fallback renderer
-   GPU renderer
-   zoom / pan / tile cache
-   grid overlay
-   pixel highlight
### `ui/`
Desktop UI:
-   main window
-   toolbar
-   histogram panel
-   inspector panel
-   shortcuts
-   dialogs
---
## 9.2 Internal data model
Have a clear distinction between:
### Source data
Exact loaded pixels:
-   original bit depth
-   original layout
-   original channel meaning
### Interpretation state
How the app currently interprets the source:
-   Bayer pattern
-   AWB gains
-   CCM
-   black/white level
-   demosaic mode
### View state
How the user currently sees it:
-   zoom
-   pan
-   overlays
-   histogram visibility
-   selected channel view
This separation will save a lot of pain.
---
## 10\. Recommended phased roadmap
## Phase 1 — Minimal usable viewer
Goal: useful as soon as possible
-   open PNG / JPEG / TIFF
-   image canvas with zoom/pan
-   exact pixel inspector
-   nearest-neighbor only zoom
-   histogram panel
-   dark minimalist UI
-   recent files
-   basic metadata display
## Phase 2 — RAW foundation
-   DNG support
-   rendered sRGB mode
-   Bayer/raw mode
-   raw histogram
-   basic RAW metadata panel
## Phase 3 — Binary raw power tools
-   binary raw open dialog
-   width/height/bit-depth/Bayer controls
-   parameter persistence
-   AWB sliders
-   CCM entry
-   black/white level
-   channel views
-   filename-based parameter deduction
## Phase 4 — Performance and polish
-   GPU viewport
-   tiled rendering
-   huge image handling
-   ROI statistics
-   shortcuts polish
-   layout persistence
-   reload-on-file-change
## Phase 5 — Advanced inspection
-   compare two images
-   synchronized zoom/pan
-   diff mode
-   line profile
-   ROI histogram / stats
-   pixel probe history
---
## 11\. Nice-to-have features later
These are strong additions, but not v1 blockers:
-   split-view compare
-   blink compare
-   false color display
-   channel isolation
-   line profile plot
-   EXIF / DNG tag browser
-   LUT preview
-   raw overexposure / underexposure mask
-   Bayer phase overlay
-   scripting interface
-   plugin API
---
## 12\. Tech direction
Since you care about:
-   cross platform
-   speed
-   clean code
-   least dependency
-   modern UI
A strong direction is:
### C++ + SDL/GLFW + Dear ImGui
Pros:
-   leaner
-   very fast
-   simple mental model
-   agent-friendly
-   good for tool-style apps
Cons:
-   more custom work for polished desktop UX
-   file dialogs / docking / native behavior need care
---
## 13\. AI-agent-friendly coding rules
This is the part I’d make explicit, since you mentioned AI agent vibe coding.
### Repo rules
-   one clear repo
-   small modules
-   no giant files
-   every module has a narrow responsibility
-   avoid clever abstractions early
-   prefer obvious code over generic frameworks
### Coding rules
-   strong typed structs for image metadata and params
-   minimize hidden global state
-   every feature behind a small interface
-   log important decode/render decisions
-   deterministic behavior
-   simple error messages for bad raw settings
### Agent workflow rules
-   keep a `docs/architecture.md`
-   keep a `docs/product_spec.md`
-   keep a `docs/roadmap.md`
-   keep a `docs/decisions.md`
-   write short ADRs for major technical decisions
-   each PR / change should update docs if behavior changes
### Testing rules
-   golden image tests
-   pixel value correctness tests
-   histogram correctness tests
-   raw parameter parsing tests
-   filename deduction tests
-   zoom sampling tests to ensure no interpolation sneaks in
This one matters a lot:
**test the truth-path, not just the rendered screenshot.**
---
