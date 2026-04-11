# winget Submission Bundle

This directory contains a ready-to-submit manifest set for `microsoft/winget-pkgs`.

Current package:

- Identifier: `Woodknight.PixelScope`
- Version: `0.1.0`

Submission flow:

1. Clone `https://github.com/microsoft/winget-pkgs`
2. Copy `packaging/winget/manifests/Woodknight/PixelScope/0.1.0/` into:

```text
manifests/w/Woodknight/PixelScope/0.1.0/
```

3. Run the winget validation tooling
4. Open a PR upstream

Notes:

- The current manifest targets the existing Windows `.zip` release artifact as a portable nested installer.
- If future releases switch to NSIS `.exe`, update the installer manifest accordingly before submission.
