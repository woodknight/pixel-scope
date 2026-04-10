# Packaging

This directory contains downstream publishing metadata that maps GitHub release artifacts into package-manager submissions.

## Homebrew

- Formula scaffold: `packaging/homebrew/Formula/pixelscope.rb`
- Intended target: your own tap repository, for example `woodknight/homebrew-tap`

Typical flow:

1. Copy the formula into your tap under `Formula/pixelscope.rb`
2. Update `url`, `sha256`, and any version-specific extracted path when you cut a new release
3. `brew install woodknight/tap/pixelscope`

## winget

- Manifests: `packaging/winget/manifests/Woodknight/PixelScope/0.1.0/`
- Intended target: a PR to `microsoft/winget-pkgs`

Typical flow:

1. Copy the version folder into a local checkout of `microsoft/winget-pkgs`
2. Run the winget validation tooling
3. Submit the manifest PR upstream

## Linux AppImage

The GitHub Actions release workflow builds an AppImage on Linux using `linuxdeploy`. The produced AppImage is intended for direct download from GitHub Releases.
