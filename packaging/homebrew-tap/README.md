# Homebrew Tap

This folder mirrors the structure of a standalone Homebrew tap repository.

Recommended target repository:

- `woodknight/homebrew-tap`

Contents:

- `Formula/pixelscope.rb`

Publishing flow:

1. Create a tap repository such as `homebrew-tap`
2. Copy the contents of this folder to the root of that repository
3. Commit and push
4. Install with:

```bash
brew tap woodknight/tap
brew install pixelscope
```
