#pragma once

#include <optional>
#include <string>

#include "core/image.h"
#include "io/binary_raw_loader.h"

namespace pixelscope::io {

struct LoadImageResult {
  pixelscope::core::ImageData image;
  std::string error_message;

  [[nodiscard]] bool ok() const { return error_message.empty() && image.valid(); }
};

[[nodiscard]] LoadImageResult load_image_file(
    const std::string& path,
    std::optional<BinaryRawParameters> binary_raw_parameters = std::nullopt);

}  // namespace pixelscope::io
