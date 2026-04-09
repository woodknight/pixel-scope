#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "core/image.h"

namespace pixelscope::core {

struct HistogramChannel {
  std::array<std::uint32_t, 256> bins = {};
  std::uint32_t max_count = 0;
};

struct ImageHistogram {
  HistogramChannel red = {};
  HistogramChannel green = {};
  HistogramChannel blue = {};
  HistogramChannel luminance = {};
  std::size_t sample_count = 0;

  [[nodiscard]] bool empty() const { return sample_count == 0; }
};

[[nodiscard]] ImageHistogram compute_histogram(const ImageData& image);

}  // namespace pixelscope::core
