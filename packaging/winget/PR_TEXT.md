# winget PR Text

Use this when opening the upstream PR to `microsoft/winget-pkgs`.

Suggested branch name:

```text
woodknight-pixelscope-0.1.0
```

Suggested PR title:

```text
Add Woodknight.PixelScope version 0.1.0
```

Suggested PR body:

```markdown
## Summary

- add `Woodknight.PixelScope` version `0.1.0`
- package source is the GitHub release asset from `woodknight/pixel-scope`
- installer is modeled as a portable app nested inside the published `.zip`

## Release Details

- Release page: https://github.com/woodknight/pixel-scope/releases/tag/v0.1.0
- Installer URL: https://github.com/woodknight/pixel-scope/releases/download/v0.1.0/pixelscope-0.1.0-windows-x86_64.zip
- SHA256: `CE6A402CF24378E0914668CC523C45917F55F86568B2BFDDFA8AECA4D62271EE`
- Nested executable: `PixelScope-0.1.0-Windows-AMD64/bin/pixelscope.exe`

## Notes

- license: MIT
- package homepage: https://github.com/woodknight/pixel-scope
- issues: https://github.com/woodknight/pixel-scope/issues
```
