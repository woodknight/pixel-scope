#pragma once

#include <string>

#include "core/image.h"

namespace pixelscope::io {

struct LoadImageResult {
  pixelscope::core::ImageData image;
  std::string error_message;

  [[nodiscard]] bool ok() const { return error_message.empty() && image.valid(); }
};

[[nodiscard]] LoadImageResult load_image_file(const std::string& path);

}  // namespace pixelscope::io
