Implement the Phase 1 foundation for PixelScope.

Context:
- Review the documents in `docs/` before making changes.

Goal:
- Add initial DNG image support. Reference implementation: https://github.com/CyberTimon/RapidRAW

What success looks like:
- PixelScope can load and display DNG files through a simple, testable path.
- The implementation establishes the project foundation without overcommitting to a full RAW pipeline.

Constraints:
- Keep dependencies minimal.
- Preserve clear module boundaries across `ui/`, `io/`, `core/`, and `render/`.
- Do not implement a general RAW workflow yet; focus only on the minimum DNG support needed for Phase 1.
- Use nearest-neighbor rendering only.
- Add a basic, testable image model in `core/`.
- Update the README with build instructions if they change or are missing.

Deliverables:
- Working code changes.
- Brief architecture notes in `docs/architecture.md` if the implementation introduces new structure or decisions worth recording.
- A short follow-up task list for the next phase.

Coding style:
- Prefer explicit structs over deep inheritance.
- Avoid singleton or global state.
- Keep files under roughly 300 lines where practical.
- Separate source data from display data.
- Choose straightforward, testable code over framework-heavy abstractions.
- Favor boring, reliable architecture.

Tech stack:
- C++ with SDL/GLFW and Dear ImGui.

Finish by running the build and relevant tests.
If something fails, fix it.
Do not leave the repository in a broken state.
