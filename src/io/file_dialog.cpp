#include "io/file_dialog.h"

#include <array>
#include <cstdio>
#include <memory>
#include <string>

#include <tinyfiledialogs.h>

namespace pixelscope::io {

namespace {

#if defined(__linux__)
enum class ZenityDialogResult {
  kSelectedPath,
  kCancelled,
  kUnavailable,
};

struct PipeCloser {
  void operator()(FILE* file) const {
    if (file != nullptr) {
      pclose(file);
    }
  }
};

struct ZenitySelection {
  ZenityDialogResult result = ZenityDialogResult::kUnavailable;
  std::optional<std::string> path;
};

ZenitySelection open_image_dialog_with_zenity() {
  constexpr const char* command =
      "zenity --file-selection "
      "--title='Open image' "
      "--file-filter='Images | *.png *.PNG *.jpg *.JPG *.jpeg *.JPEG *.tif *.TIF *.tiff *.TIFF *.dng *.DNG *.raw *.RAW *.bin *.BIN *.bayer *.BAYER' "
      "2>/dev/null";

  std::unique_ptr<FILE, PipeCloser> pipe(popen(command, "r"));
  if (!pipe) {
    return {};
  }

  std::array<char, 4096> buffer{};
  if (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe.get()) == nullptr) {
    return {.result = ZenityDialogResult::kCancelled};
  }

  std::string path(buffer.data());
  while (!path.empty() && (path.back() == '\n' || path.back() == '\r')) {
    path.pop_back();
  }
  if (path.empty()) {
    return {.result = ZenityDialogResult::kCancelled};
  }
  return {
      .result = ZenityDialogResult::kSelectedPath,
      .path = std::move(path),
  };
}
#endif

}  // namespace

std::optional<std::string> open_image_dialog() {
#if defined(__linux__)
  const auto zenity = open_image_dialog_with_zenity();
  if (zenity.result == ZenityDialogResult::kSelectedPath) {
    return zenity.path;
  }
  if (zenity.result == ZenityDialogResult::kCancelled) {
    return std::nullopt;
  }
#endif

  const char* filters[] = {"*.png", "*.jpg", "*.jpeg", "*.tif", "*.tiff", "*.dng", "*.raw", "*.bin", "*.bayer"};
  const char* selected = tinyfd_openFileDialog(
      "Open image",
      "",
      9,
      filters,
      "PNG, JPEG, TIFF, DNG, and binary Bayer raw",
      0);
  if (selected == nullptr) {
    return std::nullopt;
  }
  return std::string(selected);
}

}  // namespace pixelscope::io
