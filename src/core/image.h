#pragma once

#include <cstddef>
#include <cstdint>
#include <array>
#include <optional>
#include <string>
#include <vector>

namespace pixelscope::core {

struct PixelRgba8 {
  std::uint8_t r = 0;
  std::uint8_t g = 0;
  std::uint8_t b = 0;
  std::uint8_t a = 255;
};

struct ImageMetadata {
  int width = 0;
  int height = 0;
  int original_channel_count = 0;
  int bits_per_channel = 8;
  bool is_raw_bayer_plane = false;
  std::array<int, 4> cfa_pattern = {-1, -1, -1, -1};
  std::string source_path;
};

class ImageData {
 public:
  ImageData() = default;
  ImageData(
      ImageMetadata metadata,
      std::vector<std::uint8_t> pixels_rgba8,
      std::vector<std::uint16_t> raw_samples = {});

  [[nodiscard]] bool valid() const;
  [[nodiscard]] const ImageMetadata& metadata() const;
  [[nodiscard]] const std::vector<std::uint8_t>& pixels_rgba8() const;
  [[nodiscard]] const std::vector<std::uint16_t>& raw_samples() const;
  [[nodiscard]] std::optional<PixelRgba8> pixel_at(int x, int y) const;
  [[nodiscard]] bool has_raw_samples() const;
  [[nodiscard]] std::optional<std::uint16_t> raw_sample_at(int x, int y) const;
  [[nodiscard]] std::size_t byte_size() const;

 private:
  ImageMetadata metadata_;
  std::vector<std::uint8_t> pixels_rgba8_;
  std::vector<std::uint16_t> raw_samples_;
};

}  // namespace pixelscope::core
