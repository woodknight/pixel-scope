# Homebrew Tap Commit Text

Use this in `woodknight/homebrew-tap` when you add the formula.

Suggested commit message:

```text
Add PixelScope 0.1.0 formula
```

Suggested PR title:

```text
Add PixelScope 0.1.0
```

Suggested PR body:

```markdown
## Summary

- add a `pixelscope` Homebrew formula
- package the current macOS arm64 release from `woodknight/pixel-scope`
- install a small wrapper so `pixelscope` launches the bundled app binary cleanly

## Release Source

- GitHub release: https://github.com/woodknight/pixel-scope/releases/tag/v0.1.0
- Artifact: `pixelscope-0.1.0-macos-arm64.zip`
- SHA256: `a4b54d710c4637ee9a35e33461d8cba9e4167b09484cb293b5a5bec037321e68`

## Notes

- the formula depends on `libtiff`
- helper bridge binaries are bundled next to the main executable inside the release archive
```
