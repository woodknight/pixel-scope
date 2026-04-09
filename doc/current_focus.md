Implement Phase 1 foundation for PixelScope.

Context:
- Read docs inside `docs/`

Task:
- Create a minimal cross-platform desktop app skeleton
- Add a main window
- Add an image canvas widget
- Add file open support for PNG and JPEG
- Add zoom and pan support
- Add pixel readout in a status bar

Constraints:
- Keep dependencies minimal
- Keep modules separated: ui/, io/, core/, render/
- Do not implement RAW yet
- Use nearest-neighbor rendering only
- Add a basic testable image model in core/
- Update README with build instructions

Deliverables:
- code changes
- brief architecture notes in docs/architecture.md if needed
- a short list of follow-up tasks

Coding style:
- prefer explicit structs over deep inheritance
- avoid singleton/global state
- keep files under ~300 lines where practical
- separate source data from display data
- write obvious code, not framework magic
- prefer boring, testable architecture

Tech stack:
- C++ + SDL/GLFW + Dear ImGui


Implement the feature, then run the build and relevant tests.
If something fails, fix it.
Do not leave the repo in a broken state.