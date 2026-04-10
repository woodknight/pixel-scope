#include "io/image_loader.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

#include "io/binary_raw_loader.h"
#include "io/dng_loader.h"
#include "io/metadata_loader.h"
#include "io/tiff_loader.h"

namespace pixelscope::io {

namespace {

bool is_supported_extension(const std::string& path) {
  const auto last_dot = path.find_last_of('.');
  if (last_dot == std::string::npos) {
    return false;
  }

  std::string extension = path.substr(last_dot);
  std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return extension == ".png" || extension == ".jpg" || extension == ".jpeg" || extension == ".tif" ||
         extension == ".tiff" || extension == ".dng" || is_binary_raw_file_path(path);
}

std::string normalized_extension(const std::string& path) {
  const auto last_dot = path.find_last_of('.');
  if (last_dot == std::string::npos) {
    return {};
  }

  std::string extension = path.substr(last_dot);
  std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return extension;
}

}  // namespace

LoadImageResult load_image_file(
    const std::string& path,
    std::optional<BinaryRawParameters> binary_raw_parameters) {
  const std::string extension = normalized_extension(path);

  if (!is_supported_extension(path)) {
    return {.error_message = "Only PNG, JPEG, TIFF, DNG, and binary Bayer raw files are supported."};
  }

  if (extension == ".dng") {
    auto dng_result = load_dng_file(path);
    return {.image = std::move(dng_result.image), .error_message = std::move(dng_result.error_message)};
  }

  if (is_binary_raw_file_path(path)) {
    if (!binary_raw_parameters.has_value()) {
      return {.error_message = "Binary raw import needs width, height, CFA pattern, endianness, and bit width."};
    }
    auto raw_result = load_binary_raw_file(path, *binary_raw_parameters);
    return {.image = std::move(raw_result.image), .error_message = std::move(raw_result.error_message)};
  }

  if (extension == ".tif" || extension == ".tiff") {
    return load_tiff_file(path);
  }

  int width = 0;
  int height = 0;
  int channels_in_file = 0;
  unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels_in_file, STBI_rgb_alpha);
  if (data == nullptr) {
    return {.error_message = stbi_failure_reason()};
  }

  const std::size_t byte_count = static_cast<std::size_t>(width * height * 4);
  std::vector<std::uint8_t> pixels(data, data + byte_count);
  stbi_image_free(data);

  pixelscope::core::ImageMetadata metadata{
      .width = width,
      .height = height,
      .original_channel_count = channels_in_file,
      .bits_per_channel = 8,
      .metadata_entries = load_embedded_metadata(path),
      .source_path = path,
  };
  return {.image = pixelscope::core::ImageData(std::move(metadata), std::move(pixels))};
}

}  // namespace pixelscope::io
