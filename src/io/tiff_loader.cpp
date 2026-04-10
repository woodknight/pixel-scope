#include "io/tiff_loader.h"

#include <tiffio.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

#include "io/metadata_loader.h"

namespace pixelscope::io {

namespace {

std::uint32_t max_value_for_bits(int bits_per_channel) {
  if (bits_per_channel <= 0) {
    return 255;
  }
  if (bits_per_channel >= 32) {
    return std::numeric_limits<std::uint32_t>::max();
  }
  return (1u << bits_per_channel) - 1u;
}

std::uint32_t read_sample(const std::vector<std::uint8_t>& bytes, std::size_t byte_offset, int bits_per_channel) {
  if (bits_per_channel <= 8) {
    return bytes[byte_offset];
  }

  std::uint16_t value = 0;
  std::memcpy(&value, bytes.data() + byte_offset, sizeof(value));
  return value;
}

std::uint8_t sample_to_u8(std::uint32_t sample, int bits_per_channel, bool invert = false) {
  const std::uint32_t max_value = max_value_for_bits(bits_per_channel);
  const std::uint32_t clamped = std::min(sample, max_value);
  const std::uint32_t normalized = invert ? (max_value - clamped) : clamped;
  if (max_value == 0) {
    return 0;
  }
  return static_cast<std::uint8_t>((normalized * 255u + (max_value / 2u)) / max_value);
}

bool is_supported_photometric(std::uint16_t photometric) {
  return photometric == PHOTOMETRIC_RGB || photometric == PHOTOMETRIC_MINISBLACK ||
         photometric == PHOTOMETRIC_MINISWHITE;
}

LoadImageResult decode_contiguous_tiff(
    TIFF* tiff,
    const std::string& path,
    int width,
    int height,
    int samples_per_pixel,
    int bits_per_channel,
    std::uint16_t photometric) {
  const tmsize_t scanline_size = TIFFScanlineSize(tiff);
  if (scanline_size <= 0) {
    return {.error_message = "Failed to determine TIFF scanline size."};
  }

  std::vector<std::uint8_t> scanline(static_cast<std::size_t>(scanline_size));
  std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u);
  std::vector<std::uint16_t> pixels16;
  if (bits_per_channel > 8) {
    pixels16.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u);
  }
  const bool invert_gray = photometric == PHOTOMETRIC_MINISWHITE;
  const std::size_t bytes_per_sample = static_cast<std::size_t>(bits_per_channel / 8);

  for (int y = 0; y < height; ++y) {
    if (TIFFReadScanline(tiff, scanline.data(), static_cast<std::uint32_t>(y), 0) < 0) {
      return {.error_message = "Failed to read TIFF scanline."};
    }

    for (int x = 0; x < width; ++x) {
      const std::size_t pixel_offset = static_cast<std::size_t>(x) * static_cast<std::size_t>(samples_per_pixel);
      const std::size_t rgba_offset =
          (static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)) * 4u;

      std::uint32_t r = 0;
      std::uint32_t g = 0;
      std::uint32_t b = 0;
      std::uint32_t a = max_value_for_bits(bits_per_channel);

      if (photometric == PHOTOMETRIC_RGB) {
        const std::size_t base_byte_offset = pixel_offset * bytes_per_sample;
        r = read_sample(scanline, base_byte_offset + 0 * bytes_per_sample, bits_per_channel);
        g = read_sample(scanline, base_byte_offset + 1 * bytes_per_sample, bits_per_channel);
        b = read_sample(scanline, base_byte_offset + 2 * bytes_per_sample, bits_per_channel);
        if (samples_per_pixel >= 4) {
          a = read_sample(scanline, base_byte_offset + 3 * bytes_per_sample, bits_per_channel);
        }
      } else {
        const std::size_t gray_byte_offset = pixel_offset * bytes_per_sample;
        const std::uint32_t gray = read_sample(scanline, gray_byte_offset, bits_per_channel);
        r = gray;
        g = gray;
        b = gray;
        if (samples_per_pixel >= 2) {
          a = read_sample(scanline, gray_byte_offset + bytes_per_sample, bits_per_channel);
        }
      }

      const bool invert_channel = invert_gray && photometric != PHOTOMETRIC_RGB;
      pixels[rgba_offset + 0] = sample_to_u8(r, bits_per_channel, invert_channel);
      pixels[rgba_offset + 1] = sample_to_u8(g, bits_per_channel, invert_channel);
      pixels[rgba_offset + 2] = sample_to_u8(b, bits_per_channel, invert_channel);
      pixels[rgba_offset + 3] = sample_to_u8(a, bits_per_channel);
      if (!pixels16.empty()) {
        pixels16[rgba_offset + 0] = static_cast<std::uint16_t>(invert_channel ? max_value_for_bits(bits_per_channel) - r : r);
        pixels16[rgba_offset + 1] = static_cast<std::uint16_t>(invert_channel ? max_value_for_bits(bits_per_channel) - g : g);
        pixels16[rgba_offset + 2] = static_cast<std::uint16_t>(invert_channel ? max_value_for_bits(bits_per_channel) - b : b);
        pixels16[rgba_offset + 3] = static_cast<std::uint16_t>(a);
      }
    }
  }

  pixelscope::core::ImageMetadata metadata{
      .width = width,
      .height = height,
      .original_channel_count = samples_per_pixel,
      .bits_per_channel = bits_per_channel,
      .metadata_entries = load_embedded_metadata(path),
      .source_path = path,
  };
  return {.image = pixelscope::core::ImageData(std::move(metadata), std::move(pixels), {}, std::move(pixels16))};
}

LoadImageResult decode_separate_tiff(
    TIFF* tiff,
    const std::string& path,
    int width,
    int height,
    int samples_per_pixel,
    int bits_per_channel,
    std::uint16_t photometric) {
  const tmsize_t scanline_size = TIFFScanlineSize(tiff);
  if (scanline_size <= 0) {
    return {.error_message = "Failed to determine TIFF scanline size."};
  }

  std::vector<std::vector<std::uint8_t>> planes(
      static_cast<std::size_t>(samples_per_pixel),
      std::vector<std::uint8_t>(static_cast<std::size_t>(scanline_size)));
  std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u);
  std::vector<std::uint16_t> pixels16;
  if (bits_per_channel > 8) {
    pixels16.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u);
  }
  const bool invert_gray = photometric == PHOTOMETRIC_MINISWHITE;
  const std::size_t bytes_per_sample = static_cast<std::size_t>(bits_per_channel / 8);

  for (int y = 0; y < height; ++y) {
    for (int sample = 0; sample < samples_per_pixel; ++sample) {
      if (TIFFReadScanline(
              tiff,
              planes[static_cast<std::size_t>(sample)].data(),
              static_cast<std::uint32_t>(y),
              static_cast<std::uint16_t>(sample)) < 0) {
        return {.error_message = "Failed to read TIFF scanline."};
      }
    }

    for (int x = 0; x < width; ++x) {
      const std::size_t sample_offset = static_cast<std::size_t>(x) * bytes_per_sample;
      const std::size_t rgba_offset =
          (static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)) * 4u;

      std::uint32_t r = 0;
      std::uint32_t g = 0;
      std::uint32_t b = 0;
      std::uint32_t a = max_value_for_bits(bits_per_channel);

      if (photometric == PHOTOMETRIC_RGB) {
        r = read_sample(planes[0], sample_offset, bits_per_channel);
        g = read_sample(planes[1], sample_offset, bits_per_channel);
        b = read_sample(planes[2], sample_offset, bits_per_channel);
        if (samples_per_pixel >= 4) {
          a = read_sample(planes[3], sample_offset, bits_per_channel);
        }
      } else {
        const std::uint32_t gray = read_sample(planes[0], sample_offset, bits_per_channel);
        r = gray;
        g = gray;
        b = gray;
        if (samples_per_pixel >= 2) {
          a = read_sample(planes[1], sample_offset, bits_per_channel);
        }
      }

      const bool invert_channel = invert_gray && photometric != PHOTOMETRIC_RGB;
      pixels[rgba_offset + 0] = sample_to_u8(r, bits_per_channel, invert_channel);
      pixels[rgba_offset + 1] = sample_to_u8(g, bits_per_channel, invert_channel);
      pixels[rgba_offset + 2] = sample_to_u8(b, bits_per_channel, invert_channel);
      pixels[rgba_offset + 3] = sample_to_u8(a, bits_per_channel);
      if (!pixels16.empty()) {
        pixels16[rgba_offset + 0] = static_cast<std::uint16_t>(invert_channel ? max_value_for_bits(bits_per_channel) - r : r);
        pixels16[rgba_offset + 1] = static_cast<std::uint16_t>(invert_channel ? max_value_for_bits(bits_per_channel) - g : g);
        pixels16[rgba_offset + 2] = static_cast<std::uint16_t>(invert_channel ? max_value_for_bits(bits_per_channel) - b : b);
        pixels16[rgba_offset + 3] = static_cast<std::uint16_t>(a);
      }
    }
  }

  pixelscope::core::ImageMetadata metadata{
      .width = width,
      .height = height,
      .original_channel_count = samples_per_pixel,
      .bits_per_channel = bits_per_channel,
      .metadata_entries = load_embedded_metadata(path),
      .source_path = path,
  };
  return {.image = pixelscope::core::ImageData(std::move(metadata), std::move(pixels), {}, std::move(pixels16))};
}

}  // namespace

LoadImageResult load_tiff_file(const std::string& path) {
  TIFF* tiff = TIFFOpen(path.c_str(), "r");
  if (tiff == nullptr) {
    return {.error_message = "Failed to open TIFF file."};
  }

  std::uint32_t width = 0;
  std::uint32_t height = 0;
  std::uint16_t bits_per_channel = 0;
  std::uint16_t samples_per_pixel = 0;
  std::uint16_t photometric = PHOTOMETRIC_MINISBLACK;
  std::uint16_t planar_config = PLANARCONFIG_CONTIG;

  const int has_dimensions = TIFFGetField(tiff, TIFFTAG_IMAGEWIDTH, &width) &&
                             TIFFGetField(tiff, TIFFTAG_IMAGELENGTH, &height);
  const int has_bits = TIFFGetField(tiff, TIFFTAG_BITSPERSAMPLE, &bits_per_channel);
  const int has_samples = TIFFGetFieldDefaulted(tiff, TIFFTAG_SAMPLESPERPIXEL, &samples_per_pixel);
  TIFFGetFieldDefaulted(tiff, TIFFTAG_PHOTOMETRIC, &photometric);
  TIFFGetFieldDefaulted(tiff, TIFFTAG_PLANARCONFIG, &planar_config);

  LoadImageResult result;
  if (!has_dimensions || width == 0 || height == 0) {
    result = {.error_message = "Unsupported TIFF: missing image dimensions."};
  } else if (!has_bits || (bits_per_channel != 8 && bits_per_channel != 16)) {
    result = {.error_message = "Unsupported TIFF: only 8-bit and 16-bit channels are supported."};
  } else if (!has_samples || samples_per_pixel == 0) {
    result = {.error_message = "Unsupported TIFF: missing sample count."};
  } else if (!is_supported_photometric(photometric)) {
    result = {.error_message = "Unsupported TIFF photometric interpretation."};
  } else {
    const int width_i = static_cast<int>(width);
    const int height_i = static_cast<int>(height);
    const int samples_i = static_cast<int>(samples_per_pixel);
    const int bits_i = static_cast<int>(bits_per_channel);

    if (photometric == PHOTOMETRIC_RGB && samples_i != 3 && samples_i != 4) {
      result = {.error_message = "Unsupported TIFF: RGB images must have 3 or 4 channels."};
    } else if ((photometric == PHOTOMETRIC_MINISBLACK || photometric == PHOTOMETRIC_MINISWHITE) &&
               samples_i != 1 && samples_i != 2) {
      result = {.error_message = "Unsupported TIFF: grayscale images must have 1 or 2 channels."};
    } else if (planar_config == PLANARCONFIG_CONTIG) {
      result = decode_contiguous_tiff(tiff, path, width_i, height_i, samples_i, bits_i, photometric);
    } else if (planar_config == PLANARCONFIG_SEPARATE) {
      result = decode_separate_tiff(tiff, path, width_i, height_i, samples_i, bits_i, photometric);
    } else {
      result = {.error_message = "Unsupported TIFF planar configuration."};
    }
  }

  TIFFClose(tiff);
  return result;
}

}  // namespace pixelscope::io
