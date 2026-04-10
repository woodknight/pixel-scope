#include "core/image_model.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <thread>
#include <utility>
#include <vector>

namespace pixelscope::core {

namespace {

constexpr std::size_t kPixelStride = 4;
constexpr std::size_t kAutoContrastParallelThresholdPixels = 1U << 20;

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

std::uint8_t rgba8_luminance_at(const std::vector<std::uint8_t>& pixels, std::size_t pixel_index) {
  const std::size_t index = pixel_index * kPixelStride;
  const std::uint32_t weighted_sum =
      77U * pixels[index + 0] + 150U * pixels[index + 1] + 29U * pixels[index + 2] + 128U;
  return static_cast<std::uint8_t>(weighted_sum >> 8U);
}

std::uint8_t percentile_from_histogram(
    const std::array<std::uint32_t, 256>& histogram,
    std::size_t sample_count,
    double percentile) {
  if (sample_count == 0) {
    return 0;
  }

  const double clamped = std::clamp(percentile, 0.0, 1.0);
  const std::size_t target_rank =
      static_cast<std::size_t>(clamped * static_cast<double>(sample_count - 1)) + 1U;

  std::size_t cumulative = 0;
  for (std::size_t value = 0; value < histogram.size(); ++value) {
    cumulative += histogram[value];
    if (cumulative >= target_rank) {
      return static_cast<std::uint8_t>(value);
    }
  }

  return 255;
}

std::array<std::uint8_t, 256> build_auto_contrast_lut(
    const std::vector<std::uint8_t>& pixels,
    double shadow_clip_percent,
    double highlight_clip_percent) {
  std::array<std::uint32_t, 256> histogram = {};
  const std::size_t pixel_count = pixels.size() / kPixelStride;
  for (std::size_t pixel_index = 0; pixel_index < pixel_count; ++pixel_index) {
    ++histogram[rgba8_luminance_at(pixels, pixel_index)];
  }

  const std::uint8_t low = percentile_from_histogram(histogram, pixel_count, shadow_clip_percent);
  const std::uint8_t high = percentile_from_histogram(histogram, pixel_count, 1.0 - highlight_clip_percent);

  std::array<std::uint8_t, 256> lut = {};
  if (high <= low) {
    for (std::size_t value = 0; value < lut.size(); ++value) {
      lut[value] = static_cast<std::uint8_t>(value);
    }
    return lut;
  }

  const int range = static_cast<int>(high) - static_cast<int>(low);
  for (int value = 0; value < 256; ++value) {
    if (value <= low) {
      lut[static_cast<std::size_t>(value)] = 0;
      continue;
    }
    if (value >= high) {
      lut[static_cast<std::size_t>(value)] = 255;
      continue;
    }

    const int stretched = ((value - static_cast<int>(low)) * 255 + (range / 2)) / range;
    lut[static_cast<std::size_t>(value)] = static_cast<std::uint8_t>(std::clamp(stretched, 0, 255));
  }

  return lut;
}

void apply_lut_range(
    std::vector<std::uint8_t>& output,
    const std::vector<std::uint8_t>& source,
    const std::array<std::uint8_t, 256>& lut,
    std::size_t begin_pixel,
    std::size_t end_pixel) {
  for (std::size_t pixel_index = begin_pixel; pixel_index < end_pixel; ++pixel_index) {
    const std::size_t index = pixel_index * kPixelStride;
    output[index + 0] = lut[source[index + 0]];
    output[index + 1] = lut[source[index + 1]];
    output[index + 2] = lut[source[index + 2]];
    output[index + 3] = source[index + 3];
  }
}

ImageData apply_auto_contrast_lut(const ImageData& source, const std::array<std::uint8_t, 256>& lut) {
  if (!source.valid()) {
    return {};
  }

  std::vector<std::uint8_t> output(source.pixels_rgba8().size());
  const std::size_t pixel_count = source.pixels_rgba8().size() / kPixelStride;
  const unsigned int worker_limit = std::max(1U, std::thread::hardware_concurrency());
  const std::size_t worker_count =
      pixel_count >= kAutoContrastParallelThresholdPixels ? std::min<std::size_t>(worker_limit, pixel_count) : 1U;

  if (worker_count == 1U) {
    apply_lut_range(output, source.pixels_rgba8(), lut, 0, pixel_count);
  } else {
    std::vector<std::thread> workers;
    workers.reserve(worker_count - 1U);
    const std::size_t chunk_size = pixel_count / worker_count;
    std::size_t begin_pixel = 0;
    for (std::size_t worker_index = 0; worker_index < worker_count; ++worker_index) {
      const std::size_t end_pixel = worker_index + 1 == worker_count ? pixel_count : begin_pixel + chunk_size;
      if (worker_index + 1 == worker_count) {
        apply_lut_range(output, source.pixels_rgba8(), lut, begin_pixel, end_pixel);
      } else {
        workers.emplace_back([&, begin_pixel, end_pixel] {
          apply_lut_range(output, source.pixels_rgba8(), lut, begin_pixel, end_pixel);
        });
      }
      begin_pixel = end_pixel;
    }

    for (auto& worker : workers) {
      worker.join();
    }
  }

  ImageMetadata metadata = source.metadata();
  return ImageData(std::move(metadata), std::move(output));
}

}  // namespace

const ImageLevel* ImageModel::pick_display_level(float zoom) const {
  if (!valid() || display_levels.empty()) {
    return nullptr;
  }

  if (zoom >= 1.0f) {
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

ImageData apply_auto_contrast(const ImageData& source, double shadow_clip_percent, double highlight_clip_percent) {
  if (!source.valid()) {
    return {};
  }

  const auto lut = build_auto_contrast_lut(source.pixels_rgba8(), shadow_clip_percent, highlight_clip_percent);
  return apply_auto_contrast_lut(source, lut);
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

ImageModel build_auto_contrast_image_model(
    const ImageModel& source_model,
    double shadow_clip_percent,
    double highlight_clip_percent) {
  ImageModel model;
  if (!source_model.valid()) {
    return model;
  }

  const auto lut =
      build_auto_contrast_lut(source_model.source.pixels_rgba8(), shadow_clip_percent, highlight_clip_percent);
  model.source = apply_auto_contrast_lut(source_model.source, lut);
  if (!model.source.valid()) {
    return {};
  }

  model.display_levels.reserve(source_model.display_levels.size());
  for (const auto& level : source_model.display_levels) {
    ImageData adjusted_level = apply_auto_contrast_lut(level.image, lut);
    if (!adjusted_level.valid()) {
      return {};
    }
    model.display_levels.push_back(ImageLevel{
        .image = std::move(adjusted_level),
        .downsample_factor = level.downsample_factor,
    });
  }

  return model;
}

}  // namespace pixelscope::core
