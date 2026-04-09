#include "io/dng_loader.h"

#define TINY_DNG_LOADER_IMPLEMENTATION
#define TINY_DNG_LOADER_NO_STB_IMAGE_INCLUDE
#include <tiny_dng_loader.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace pixelscope::io {

namespace {

template <typename T>
T read_sample(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
  T value{};
  std::memcpy(&value, bytes.data() + offset, sizeof(T));
  return value;
}

int fallback_white_level(int bits_per_sample) {
  if (bits_per_sample <= 0) {
    return 255;
  }
  if (bits_per_sample >= 31) {
    return std::numeric_limits<int>::max();
  }
  return (1 << bits_per_sample) - 1;
}

int resolve_white_level(const DngFrame& frame, int channel) {
  const int clamped_channel = std::clamp(channel, 0, 3);
  const int explicit_white = frame.white_levels[clamped_channel];
  if (explicit_white > frame.black_levels[clamped_channel]) {
    return explicit_white;
  }
  return fallback_white_level(frame.bits_per_sample);
}

std::uint8_t normalize_to_u8(std::uint32_t sample, int black_level, int white_level) {
  const int clamped_black = std::max(0, black_level);
  const int clamped_white = std::max(clamped_black + 1, white_level);
  const int numerator = std::clamp<int>(
      static_cast<int>(sample) - clamped_black,
      0,
      clamped_white - clamped_black);
  const float normalized = static_cast<float>(numerator) /
                           static_cast<float>(clamped_white - clamped_black);
  return static_cast<std::uint8_t>(std::clamp(normalized * 255.0f + 0.5f, 0.0f, 255.0f));
}

bool is_supported_frame_layout(const DngFrame& frame) {
  if (frame.width <= 0 || frame.height <= 0) {
    return false;
  }
  if (frame.samples_per_pixel != 1 && frame.samples_per_pixel != 3 && frame.samples_per_pixel != 4) {
    return false;
  }
  if (frame.bits_per_sample != 8 && frame.bits_per_sample != 16) {
    return false;
  }

  const std::size_t bytes_per_sample = static_cast<std::size_t>(frame.bits_per_sample / 8);
  const std::size_t expected_size = static_cast<std::size_t>(frame.width) *
                                    static_cast<std::size_t>(frame.height) *
                                    static_cast<std::size_t>(frame.samples_per_pixel) * bytes_per_sample;
  return frame.decoded_bytes.size() == expected_size;
}

std::uint32_t read_frame_sample(const DngFrame& frame, std::size_t sample_index) {
  const std::size_t bytes_per_sample = static_cast<std::size_t>(frame.bits_per_sample / 8);
  const std::size_t byte_offset = sample_index * bytes_per_sample;
  if (frame.bits_per_sample == 8) {
    return frame.decoded_bytes[byte_offset];
  }
  return read_sample<std::uint16_t>(frame.decoded_bytes, byte_offset);
}

DngFrame to_frame(const tinydng::DNGImage& image) {
  DngFrame frame;
  frame.width = image.width;
  frame.height = image.height;
  frame.samples_per_pixel = image.samples_per_pixel;
  frame.bits_per_sample = image.bits_per_sample;
  frame.decoded_bytes = image.data;
  for (int channel = 0; channel < 4; ++channel) {
    frame.black_levels[static_cast<std::size_t>(channel)] = image.black_level[channel];
    frame.white_levels[static_cast<std::size_t>(channel)] = image.white_level[channel];
  }
  return frame;
}

const tinydng::DNGImage* pick_primary_image(const std::vector<tinydng::DNGImage>& images) {
  const tinydng::DNGImage* best = nullptr;
  std::size_t best_area = 0;
  for (const auto& image : images) {
    if (image.data.empty()) {
      continue;
    }
    if (image.samples_per_pixel != 1 && image.samples_per_pixel != 3 && image.samples_per_pixel != 4) {
      continue;
    }
    if (image.bits_per_sample != 8 && image.bits_per_sample != 16) {
      continue;
    }

    const std::size_t area = static_cast<std::size_t>(image.width) * static_cast<std::size_t>(image.height);
    if (best == nullptr || area > best_area) {
      best = &image;
      best_area = area;
    }
  }
  return best;
}

}  // namespace

pixelscope::core::ImageData rgba8_image_from_dng_frame(
    const DngFrame& frame,
    const std::string& source_path) {
  if (!is_supported_frame_layout(frame)) {
    return {};
  }

  std::vector<std::uint8_t> rgba_pixels(static_cast<std::size_t>(frame.width * frame.height * 4), 255);
  const std::size_t pixel_count = static_cast<std::size_t>(frame.width) * static_cast<std::size_t>(frame.height);
  for (std::size_t pixel_index = 0; pixel_index < pixel_count; ++pixel_index) {
    const std::size_t source_base = pixel_index * static_cast<std::size_t>(frame.samples_per_pixel);
    const std::size_t output_base = pixel_index * 4;

    if (frame.samples_per_pixel == 1) {
      const auto value = normalize_to_u8(
          read_frame_sample(frame, source_base),
          frame.black_levels[0],
          resolve_white_level(frame, 0));
      rgba_pixels[output_base + 0] = value;
      rgba_pixels[output_base + 1] = value;
      rgba_pixels[output_base + 2] = value;
      continue;
    }

    for (int channel = 0; channel < 3; ++channel) {
      rgba_pixels[output_base + static_cast<std::size_t>(channel)] = normalize_to_u8(
          read_frame_sample(frame, source_base + static_cast<std::size_t>(channel)),
          frame.black_levels[static_cast<std::size_t>(channel)],
          resolve_white_level(frame, channel));
    }

    if (frame.samples_per_pixel == 4) {
      rgba_pixels[output_base + 3] = normalize_to_u8(
          read_frame_sample(frame, source_base + 3),
          frame.black_levels[3],
          resolve_white_level(frame, 3));
    }
  }

  pixelscope::core::ImageMetadata metadata{
      .width = frame.width,
      .height = frame.height,
      .original_channel_count = frame.samples_per_pixel,
      .bits_per_channel = frame.bits_per_sample,
      .source_path = source_path,
  };
  return pixelscope::core::ImageData(std::move(metadata), std::move(rgba_pixels));
}

DngLoadResult load_dng_file(const std::string& path) {
  std::vector<tinydng::FieldInfo> custom_fields;
  std::vector<tinydng::DNGImage> images;
  std::string warning_message;
  std::string error_message;
  if (!tinydng::LoadDNG(path.c_str(), custom_fields, &images, &warning_message, &error_message)) {
    if (!error_message.empty()) {
      return {.error_message = error_message};
    }
    return {.error_message = "Failed to decode DNG image."};
  }

  const tinydng::DNGImage* primary_image = pick_primary_image(images);
  if (primary_image == nullptr) {
    return {.error_message = "No supported 8-bit or 16-bit DNG image payload was found."};
  }

  auto image = rgba8_image_from_dng_frame(to_frame(*primary_image), path);
  if (!image.valid()) {
    return {.error_message = "DNG decode succeeded, but PixelScope could not convert the image to RGBA8."};
  }

  return {.image = std::move(image)};
}

}  // namespace pixelscope::io
