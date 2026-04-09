#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "core/image.h"

namespace pixelscope::io {

struct DngLoadResult {
  pixelscope::core::ImageData image;
  std::string error_message;

  [[nodiscard]] bool ok() const { return error_message.empty() && image.valid(); }
};

struct DngFrame {
  int width = 0;
  int height = 0;
  int samples_per_pixel = 0;
  int bits_per_sample = 0;
  int original_bits_per_sample = 0;
  std::array<int, 4> cfa_pattern = {-1, -1, -1, -1};
  std::array<int, 4> black_levels = {0, 0, 0, 0};
  std::array<int, 4> white_levels = {-1, -1, -1, -1};
  std::vector<std::uint8_t> decoded_bytes;
};

[[nodiscard]] pixelscope::core::ImageData rgba8_image_from_dng_frame(
    const DngFrame& frame,
    const std::string& source_path);
[[nodiscard]] DngLoadResult load_dng_file(const std::string& path);

}  // namespace pixelscope::io
