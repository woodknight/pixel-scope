#include "core/image_model.h"

#include <algorithm>
#include <cstddef>
#include <utility>
#include <vector>

namespace pixelscope::core {

namespace {

std::vector<std::uint8_t> copy_nearest_rgba8(
    const std::vector<std::uint8_t>& source_pixels,
    int source_width,
    int source_height,
    int output_width,
    int output_height) {
  std::vector<std::uint8_t> output(static_cast<std::size_t>(output_width * output_height * 4));
  for (int y = 0; y < output_height; ++y) {
    const int source_y = std::min(source_height - 1, y * 2);
    for (int x = 0; x < output_width; ++x) {
      const int source_x = std::min(source_width - 1, x * 2);
      const std::size_t source_index = static_cast<std::size_t>((source_y * source_width + source_x) * 4);
      const std::size_t output_index = static_cast<std::size_t>((y * output_width + x) * 4);
      output[output_index + 0] = source_pixels[source_index + 0];
      output[output_index + 1] = source_pixels[source_index + 1];
      output[output_index + 2] = source_pixels[source_index + 2];
      output[output_index + 3] = source_pixels[source_index + 3];
    }
  }
  return output;
}

}  // namespace

const ImageLevel* ImageModel::pick_display_level(float zoom) const {
  if (!valid() || display_levels.empty()) {
    return nullptr;
  }

  const ImageLevel* fallback = &display_levels.back();
  for (const auto& level : display_levels) {
    if (zoom * static_cast<float>(level.downsample_factor) >= 1.0f) {
      return &level;
    }
  }
  return fallback;
}

ImageData downsample_nearest_2x(const ImageData& source) {
  if (!source.valid()) {
    return {};
  }

  const auto& metadata = source.metadata();
  if (metadata.width <= 1 && metadata.height <= 1) {
    return source;
  }

  const int output_width = std::max(1, (metadata.width + 1) / 2);
  const int output_height = std::max(1, (metadata.height + 1) / 2);
  ImageMetadata output_metadata = metadata;
  output_metadata.width = output_width;
  output_metadata.height = output_height;

  return ImageData(
      std::move(output_metadata),
      copy_nearest_rgba8(
          source.pixels_rgba8(),
          metadata.width,
          metadata.height,
          output_width,
          output_height));
}

ImageModel build_image_model(ImageData source, int max_display_dimension) {
  ImageModel model;
  if (!source.valid()) {
    return model;
  }

  model.source = std::move(source);
  if (max_display_dimension <= 0) {
    return model;
  }

  ImageData current = model.source;
  int factor = 1;
  while (std::max(current.metadata().width, current.metadata().height) > max_display_dimension) {
    current = downsample_nearest_2x(current);
    factor *= 2;
    if (!current.valid()) {
      model.display_levels.clear();
      return model;
    }
    model.display_levels.push_back(ImageLevel{
        .image = current,
        .downsample_factor = factor,
    });
  }

  return model;
}

}  // namespace pixelscope::core
