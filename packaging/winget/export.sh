#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
src_dir="${repo_root}/packaging/winget/manifests/Woodknight/PixelScope/0.1.0"
dest_root="${1:-${repo_root}/packaging/winget/out}"
dest_dir="${dest_root}/manifests/w/Woodknight/PixelScope/0.1.0"

mkdir -p "${dest_dir}"
cp "${src_dir}"/*.yaml "${dest_dir}/"

echo "Exported winget manifests to ${dest_dir}"
