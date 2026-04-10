#pragma once

#include <cstddef>
#include <cstdint>

#include "core/image.h"

namespace pixelscope::core {

struct ImageStatistics {
  std::uint32_t min_value = 0;
  std::uint32_t max_value = 0;
  std::uint32_t percentile_10 = 0;
  std::uint32_t median = 0;
  std::uint32_t percentile_90 = 0;
  double mean = 0.0;
  std::size_t sample_count = 0;
  bool uses_raw_samples = false;
  bool uses_high_precision_luminance = false;

  [[nodiscard]] bool empty() const { return sample_count == 0; }
};

[[nodiscard]] ImageStatistics compute_image_statistics(const ImageData& image);

}  // namespace pixelscope::core
