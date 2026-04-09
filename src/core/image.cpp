#include "core/image.h"

namespace pixelscope::core {

ImageData::ImageData(
    ImageMetadata metadata,
    std::vector<std::uint8_t> pixels_rgba8,
    std::vector<std::uint16_t> raw_samples,
    std::vector<std::uint16_t> pixels_rgba16)
    : metadata_(std::move(metadata)),
      pixels_rgba8_(std::move(pixels_rgba8)),
      raw_samples_(std::move(raw_samples)),
      pixels_rgba16_(std::move(pixels_rgba16)) {}

bool ImageData::valid() const {
  return metadata_.width > 0 && metadata_.height > 0 &&
         pixels_rgba8_.size() == static_cast<std::size_t>(metadata_.width * metadata_.height * 4) &&
         (raw_samples_.empty() ||
          raw_samples_.size() == static_cast<std::size_t>(metadata_.width * metadata_.height)) &&
         (pixels_rgba16_.empty() ||
          pixels_rgba16_.size() == static_cast<std::size_t>(metadata_.width * metadata_.height * 4));
}

const ImageMetadata& ImageData::metadata() const { return metadata_; }

const std::vector<std::uint8_t>& ImageData::pixels_rgba8() const { return pixels_rgba8_; }

const std::vector<std::uint16_t>& ImageData::raw_samples() const { return raw_samples_; }

const std::vector<std::uint16_t>& ImageData::pixels_rgba16() const { return pixels_rgba16_; }

std::optional<PixelRgba8> ImageData::pixel_at(int x, int y) const {
  if (!valid() || x < 0 || y < 0 || x >= metadata_.width || y >= metadata_.height) {
    return std::nullopt;
  }

  const std::size_t index = static_cast<std::size_t>((y * metadata_.width + x) * 4);
  return PixelRgba8{
      .r = pixels_rgba8_[index + 0],
      .g = pixels_rgba8_[index + 1],
      .b = pixels_rgba8_[index + 2],
      .a = pixels_rgba8_[index + 3],
  };
}

std::optional<PixelRgba16> ImageData::pixel16_at(int x, int y) const {
  if (!valid() || pixels_rgba16_.empty() || x < 0 || y < 0 || x >= metadata_.width || y >= metadata_.height) {
    return std::nullopt;
  }

  const std::size_t index = static_cast<std::size_t>((y * metadata_.width + x) * 4);
  return PixelRgba16{
      .r = pixels_rgba16_[index + 0],
      .g = pixels_rgba16_[index + 1],
      .b = pixels_rgba16_[index + 2],
      .a = pixels_rgba16_[index + 3],
  };
}

bool ImageData::has_raw_samples() const { return !raw_samples_.empty(); }

bool ImageData::has_pixels_rgba16() const { return !pixels_rgba16_.empty(); }

std::optional<std::uint16_t> ImageData::raw_sample_at(int x, int y) const {
  if (!valid() || raw_samples_.empty() || x < 0 || y < 0 || x >= metadata_.width || y >= metadata_.height) {
    return std::nullopt;
  }

  const std::size_t index = static_cast<std::size_t>(y * metadata_.width + x);
  return raw_samples_[index];
}

std::size_t ImageData::byte_size() const {
  return pixels_rgba8_.size() + (raw_samples_.size() * sizeof(std::uint16_t)) +
         (pixels_rgba16_.size() * sizeof(std::uint16_t));
}

}  // namespace pixelscope::core
