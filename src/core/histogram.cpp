#include "core/histogram.h"

#include <algorithm>

namespace pixelscope::core {

namespace {

void add_sample(HistogramChannel& channel, std::uint8_t value) {
  const auto index = static_cast<std::size_t>(value);
  const std::uint32_t next_count = ++channel.bins[index];
  channel.max_count = std::max(channel.max_count, next_count);
}

std::uint8_t compute_luminance(const PixelRgba8& pixel) {
  const int weighted_sum = 77 * static_cast<int>(pixel.r) + 150 * static_cast<int>(pixel.g) +
                           29 * static_cast<int>(pixel.b) + 128;
  return static_cast<std::uint8_t>(weighted_sum / 256);
}

}  // namespace

ImageHistogram compute_histogram(const ImageData& image) {
  ImageHistogram histogram;
  if (!image.valid()) {
    return histogram;
  }

  const auto& pixels = image.pixels_rgba8();
  histogram.sample_count = static_cast<std::size_t>(image.metadata().width * image.metadata().height);
  for (std::size_t index = 0; index + 3 < pixels.size(); index += 4) {
    const PixelRgba8 pixel{
        .r = pixels[index + 0],
        .g = pixels[index + 1],
        .b = pixels[index + 2],
        .a = pixels[index + 3],
    };
    add_sample(histogram.red, pixel.r);
    add_sample(histogram.green, pixel.g);
    add_sample(histogram.blue, pixel.b);
    add_sample(histogram.luminance, compute_luminance(pixel));
  }

  return histogram;
}

}  // namespace pixelscope::core
