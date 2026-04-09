#include "io/file_dialog.h"

#include <tinyfiledialogs.h>

namespace pixelscope::io {

std::optional<std::string> open_image_dialog() {
  const char* filters[] = {"*.png", "*.jpg", "*.jpeg"};
  const char* selected = tinyfd_openFileDialog(
      "Open image",
      "",
      3,
      filters,
      "PNG and JPEG",
      0);
  if (selected == nullptr) {
    return std::nullopt;
  }
  return std::string(selected);
}

}  // namespace pixelscope::io
