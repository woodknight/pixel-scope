#include "io/file_dialog.h"

#include <array>
#include <cstdio>
#include <memory>
#include <string>

#include <tinyfiledialogs.h>

namespace pixelscope::io {

namespace {

#if defined(__linux__)
struct PipeCloser {
  void operator()(FILE* file) const {
    if (file != nullptr) {
      pclose(file);
    }
  }
};

std::optional<std::string> open_image_dialog_with_zenity() {
  constexpr const char* command =
      "zenity --file-selection "
      "--title='Open image' "
      "--file-filter='Images | *.png *.PNG *.jpg *.JPG *.jpeg *.JPEG *.dng *.DNG' "
      "2>/dev/null";

  std::unique_ptr<FILE, PipeCloser> pipe(popen(command, "r"));
  if (!pipe) {
    return std::nullopt;
  }

  std::array<char, 4096> buffer{};
  if (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe.get()) == nullptr) {
    return std::nullopt;
  }

  std::string path(buffer.data());
  while (!path.empty() && (path.back() == '\n' || path.back() == '\r')) {
    path.pop_back();
  }
  if (path.empty()) {
    return std::nullopt;
  }
  return path;
}
#endif

}  // namespace

std::optional<std::string> open_image_dialog() {
#if defined(__linux__)
  if (const auto path = open_image_dialog_with_zenity()) {
    return path;
  }
#endif

  const char* filters[] = {"*.png", "*.jpg", "*.jpeg", "*.dng"};
  const char* selected = tinyfd_openFileDialog(
      "Open image",
      "",
      4,
      filters,
      "PNG, JPEG, and DNG",
      0);
  if (selected == nullptr) {
    return std::nullopt;
  }
  return std::string(selected);
}

}  // namespace pixelscope::io
