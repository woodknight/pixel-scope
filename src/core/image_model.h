#pragma once

#include <vector>

#include "core/image.h"

namespace pixelscope::core {

struct ImageLevel {
  ImageData image;
  int downsample_factor = 1;

  [[nodiscard]] bool valid() const { return downsample_factor >= 1 && image.valid(); }
};

struct ImageModel {
  ImageData source;
  std::vector<ImageLevel> display_levels;

  [[nodiscard]] bool valid() const { return source.valid(); }
  [[nodiscard]] const ImageLevel* pick_display_level(float zoom) const;
};

[[nodiscard]] ImageData downsample_nearest_2x(const ImageData& source);
[[nodiscard]] ImageModel build_image_model(ImageData source, int max_display_dimension = 2048);

}  // namespace pixelscope::core
