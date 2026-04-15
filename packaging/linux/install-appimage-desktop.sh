#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'USAGE'
Usage: install-appimage-desktop.sh <path-to-pixelscope.AppImage>

Installs PixelScope's AppImage, desktop entry, and icon for the current user.
USAGE
}

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
  usage
  exit 0
fi

if [[ $# -ne 1 ]]; then
  usage >&2
  exit 1
fi

appimage_source=$1
if [[ ! -f "$appimage_source" ]]; then
  echo "AppImage not found: $appimage_source" >&2
  exit 1
fi

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo_root=$(cd -- "$script_dir/../.." 2>/dev/null && pwd || true)

bin_dir="$HOME/.local/bin"
apps_dir="$HOME/.local/share/applications"
icon_dir="$HOME/.local/share/icons/hicolor/256x256/apps"

appimage_target="$bin_dir/pixelscope.AppImage"
desktop_target="$apps_dir/pixelscope.desktop"
icon_target="$icon_dir/pixelscope.png"

install -d "$bin_dir" "$apps_dir" "$icon_dir"
install -m 0755 "$appimage_source" "$appimage_target"

if [[ -n "$repo_root" && -f "$repo_root/assets/icon/icon_256x256.png" ]]; then
  install -m 0644 "$repo_root/assets/icon/icon_256x256.png" "$icon_target"
else
  curl -fsSL \
    https://raw.githubusercontent.com/woodknight/pixel-scope/master/assets/icon/icon_256x256.png \
    -o "$icon_target"
fi

if [[ -f "$script_dir/pixelscope.desktop" ]]; then
  install -m 0644 "$script_dir/pixelscope.desktop" "$desktop_target"
else
  curl -fsSL \
    https://raw.githubusercontent.com/woodknight/pixel-scope/master/packaging/linux/pixelscope.desktop \
    -o "$desktop_target"
fi

sed -i \
  "s|^Exec=.*|Exec=env SDL_VIDEO_X11_WMCLASS=pixelscope SDL_VIDEO_WAYLAND_WMCLASS=pixelscope SDL_APP_NAME=PixelScope $appimage_target %U|" \
  "$desktop_target"
sed -i "s|^Icon=.*|Icon=$icon_target|" "$desktop_target"
sed -i "s|^StartupNotify=.*|StartupNotify=false|" "$desktop_target"

update-desktop-database "$apps_dir" 2>/dev/null || true
gtk-update-icon-cache "$HOME/.local/share/icons/hicolor" 2>/dev/null || true

echo "Installed PixelScope desktop entry for $appimage_target"
