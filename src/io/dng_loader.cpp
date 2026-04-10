#include "io/dng_loader.h"
#include "io/metadata_loader.h"
#include "platform/runtime_paths.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#ifndef PIXELSCOPE_RAWLOADER_BRIDGE_PATH
#define PIXELSCOPE_RAWLOADER_BRIDGE_PATH "rawloader_bridge"
#endif

#ifndef PIXELSCOPE_RAWLOADER_BRIDGE_FILENAME
#define PIXELSCOPE_RAWLOADER_BRIDGE_FILENAME "rawloader_bridge"
#endif

namespace pixelscope::io {

namespace {

constexpr std::array<char, 8> kRawloaderMagic = {'P', 'S', 'R', 'D', 'N', 'G', '1', '\0'};

template <typename T>
T read_sample(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
  T value{};
  std::memcpy(&value, bytes.data() + offset, sizeof(T));
  return value;
}

template <typename T>
bool read_value(std::ifstream& stream, T& value) {
  stream.read(reinterpret_cast<char*>(&value), sizeof(T));
  return stream.good();
}

std::uint16_t read_u16_le(std::ifstream& stream, bool& ok) {
  std::uint16_t value = 0;
  ok = read_value(stream, value);
  return value;
}

std::uint32_t read_u32_le(std::ifstream& stream, bool& ok) {
  std::uint32_t value = 0;
  ok = read_value(stream, value);
  return value;
}

std::uint64_t read_u64_le(std::ifstream& stream, bool& ok) {
  std::uint64_t value = 0;
  ok = read_value(stream, value);
  return value;
}

float read_f32_le(std::ifstream& stream, bool& ok) {
  float value = 0.0f;
  ok = read_value(stream, value);
  return value;
}

std::int32_t read_i32_le(std::ifstream& stream, bool& ok) {
  std::int32_t value = 0;
  ok = read_value(stream, value);
  return value;
}

std::string read_string(std::ifstream& stream, bool& ok) {
  const auto size = read_u32_le(stream, ok);
  if (!ok) {
    return {};
  }

  std::string value(size, '\0');
  if (size > 0) {
    stream.read(value.data(), static_cast<std::streamsize>(size));
    ok = stream.good();
  }
  return value;
}

std::string format_float(float value, int precision = 4) {
  if (!std::isfinite(value)) {
    return "-";
  }

  std::ostringstream stream;
  stream << std::fixed << std::setprecision(precision) << value;
  std::string formatted = stream.str();
  while (!formatted.empty() && formatted.back() == '0') {
    formatted.pop_back();
  }
  if (!formatted.empty() && formatted.back() == '.') {
    formatted.pop_back();
  }
  if (formatted.empty()) {
    return "0";
  }
  return formatted;
}

std::string format_channel_levels(const std::array<int, 4>& values) {
  std::ostringstream stream;
  stream << "R " << values[0] << " | G1 " << values[1] << " | B " << values[2] << " | G2 " << values[3];
  return stream.str();
}

std::string format_cfa_pattern(const std::array<int, 4>& pattern) {
  const auto channel_name = [](int channel) {
    switch (channel) {
      case 0:
        return 'R';
      case 1:
        return 'G';
      case 2:
        return 'B';
      default:
        return '?';
    }
  };

  std::string label;
  label.reserve(4);
  for (int channel : pattern) {
    label.push_back(channel_name(channel));
  }
  return label;
}

std::string format_wb_coeffs(const std::array<float, 4>& values) {
  std::ostringstream stream;
  stream << "R " << format_float(values[0]) << " | G1 " << format_float(values[1]) << " | B " << format_float(values[2]);
  if (std::isfinite(values[3]) && values[3] > 0.0f) {
    stream << " | G2 " << format_float(values[3]);
  }
  return stream.str();
}

std::string format_crop_values(const std::array<int, 4>& crops) {
  std::ostringstream stream;
  stream << "Top " << crops[0] << " | Right " << crops[1] << " | Bottom " << crops[2] << " | Left " << crops[3];
  return stream.str();
}

const char* orientation_label(int orientation) {
  switch (orientation) {
    case 1:
      return "Normal";
    case 2:
      return "Mirror Horizontal";
    case 3:
      return "Rotate 180";
    case 4:
      return "Mirror Vertical";
    case 5:
      return "Mirror Horizontal + Rotate 270 CW";
    case 6:
      return "Rotate 90 CW";
    case 7:
      return "Mirror Horizontal + Rotate 90 CW";
    case 8:
      return "Rotate 270 CW";
    default:
      return "Unknown";
  }
}

std::vector<pixelscope::core::MetadataEntry> build_dng_metadata_entries(const DngFrame& frame) {
  std::vector<pixelscope::core::MetadataEntry> entries;
  entries.reserve(frame.metadata_entries.size() + 12);

  const auto push_if_value = [&](std::string label, const std::string& value) {
    if (!value.empty()) {
      entries.push_back({.label = std::move(label), .value = value});
    }
  };

  push_if_value("Camera Make", frame.make);
  push_if_value("Camera Model", frame.model);
  if (!frame.clean_make.empty() || !frame.clean_model.empty()) {
    push_if_value("Camera", frame.clean_make + (frame.clean_make.empty() || frame.clean_model.empty() ? "" : " ") + frame.clean_model);
  }
  push_if_value("CFA Pattern", format_cfa_pattern(frame.cfa_pattern));
  push_if_value("AWB Gains", format_wb_coeffs(frame.wb_coeffs));
  push_if_value("Black Levels", format_channel_levels(frame.black_levels));
  push_if_value("White Levels", format_channel_levels(frame.white_levels));
  push_if_value("Crop", format_crop_values(frame.crops));
  push_if_value("Orientation", orientation_label(frame.orientation));

  for (const auto& entry : frame.metadata_entries) {
    if (!entry.label.empty() && !entry.value.empty()) {
      entries.push_back(entry);
    }
  }

  return entries;
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
  return fallback_white_level(frame.original_bits_per_sample > 0 ? frame.original_bits_per_sample
                                                                 : frame.bits_per_sample);
}

std::uint8_t raw_plane_to_u8(std::uint32_t sample, int original_bits_per_sample) {
  const int bits = std::max(1, original_bits_per_sample);
  if (bits <= 8) {
    return static_cast<std::uint8_t>(std::min<std::uint32_t>(sample, 255));
  }
  const int shift = bits - 8;
  return static_cast<std::uint8_t>(std::min<std::uint32_t>(sample >> shift, 255));
}

std::uint8_t normalize_raw_plane_to_u8(std::uint16_t sample, std::uint16_t black_level, std::uint16_t white_level) {
  if (white_level <= black_level) {
    return raw_plane_to_u8(sample, 16);
  }

  const std::uint32_t numerator = std::clamp<std::uint32_t>(
      static_cast<std::uint32_t>(sample) - static_cast<std::uint32_t>(black_level),
      0U,
      static_cast<std::uint32_t>(white_level) - static_cast<std::uint32_t>(black_level));
  const std::uint32_t denominator = static_cast<std::uint32_t>(white_level) - static_cast<std::uint32_t>(black_level);
  return static_cast<std::uint8_t>((numerator * 255U + (denominator / 2U)) / denominator);
}

pixelscope::core::PixelRgba8 cfa_mosaic_pixel(
    std::uint8_t value,
    const std::array<int, 4>& cfa_pattern,
    int x,
    int y) {
  const int pattern_index = (y % 2) * 2 + (x % 2);
  const int channel = cfa_pattern[static_cast<std::size_t>(pattern_index)];
  if (channel == 0) {
    return {.r = value, .g = 0, .b = 0, .a = 255};
  }
  if (channel == 1) {
    return {.r = 0, .g = value, .b = 0, .a = 255};
  }
  if (channel == 2) {
    return {.r = 0, .g = 0, .b = value, .a = 255};
  }
  return {.r = value, .g = value, .b = value, .a = 255};
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

std::filesystem::path temp_output_path(const char* suffix) {
  namespace fs = std::filesystem;

  std::error_code error_code;
  fs::path directory = fs::temp_directory_path(error_code);
  if (error_code) {
    directory = fs::current_path(error_code);
  }
  if (directory.empty()) {
    directory = ".";
  }

  const auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
  for (int attempt = 0; attempt < 64; ++attempt) {
    const auto candidate = directory /
                           ("pixelscope_rawloader_" + std::to_string(timestamp) + "_" +
                            std::to_string(attempt) + suffix);
    if (!fs::exists(candidate, error_code)) {
      return candidate;
    }
  }
  return directory / ("pixelscope_rawloader_fallback" + std::string(suffix));
}

std::string shell_quote(const std::string& value) {
#ifdef _WIN32
  std::string quoted = "\"";
  for (char c : value) {
    if (c == '"') {
      quoted += "\\\"";
    } else {
      quoted += c;
    }
  }
  quoted += "\"";
  return quoted;
#else
  std::string quoted = "'";
  for (char c : value) {
    if (c == '\'') {
      quoted += "'\"'\"'";
    } else {
      quoted += c;
    }
  }
  quoted += "'";
  return quoted;
#endif
}

struct ScopedFileCleanup {
  std::filesystem::path path;

  ~ScopedFileCleanup() {
    if (!path.empty()) {
      std::error_code error_code;
      std::filesystem::remove(path, error_code);
    }
  }
};

bool run_rawloader_bridge(
    const std::string& input_path,
    const std::filesystem::path& output_path,
    const std::filesystem::path& error_path,
    std::string& error_message) {
  const auto bridge_path = pixelscope::platform::resolve_companion_binary(
      PIXELSCOPE_RAWLOADER_BRIDGE_FILENAME,
      PIXELSCOPE_RAWLOADER_BRIDGE_PATH);
  const std::string command = shell_quote(bridge_path.string()) + " " +
                              shell_quote(input_path) + " " +
                              shell_quote(output_path.string()) + " 2> " +
                              shell_quote(error_path.string());
  const int exit_code = std::system(command.c_str());
  if (exit_code == 0) {
    return true;
  }

  std::ifstream error_stream(error_path);
  if (error_stream.good()) {
    std::ostringstream buffer;
    buffer << error_stream.rdbuf();
    error_message = buffer.str();
  }
  if (error_message.empty()) {
    error_message = "rawloader bridge failed to decode the DNG file.";
  }
  while (!error_message.empty() &&
         (error_message.back() == '\n' || error_message.back() == '\r')) {
    error_message.pop_back();
  }
  return false;
}

bool load_frame_from_payload(const std::filesystem::path& payload_path, DngFrame& frame, std::string& error_message) {
  std::ifstream stream(payload_path, std::ios::binary);
  if (!stream.is_open()) {
    error_message = "PixelScope could not read the rawloader payload.";
    return false;
  }

  std::array<char, 8> magic{};
  stream.read(magic.data(), static_cast<std::streamsize>(magic.size()));
  if (stream.gcount() != static_cast<std::streamsize>(magic.size()) || magic != kRawloaderMagic) {
    error_message = "PixelScope received an invalid rawloader payload header.";
    return false;
  }

  bool ok = true;
  const auto version = read_u32_le(stream, ok);
  if (!ok || (version != 1 && version != 2)) {
    error_message = "PixelScope received an unsupported rawloader payload version.";
    return false;
  }

  frame.width = static_cast<int>(read_u32_le(stream, ok));
  if (!ok) {
    error_message = "PixelScope could not read the rawloader payload dimensions.";
    return false;
  }
  frame.height = static_cast<int>(read_u32_le(stream, ok));
  frame.samples_per_pixel = static_cast<int>(read_u32_le(stream, ok));
  frame.bits_per_sample = static_cast<int>(read_u32_le(stream, ok));
  frame.original_bits_per_sample = static_cast<int>(read_u32_le(stream, ok));
  if (!ok) {
    error_message = "PixelScope could not read the rawloader payload metadata.";
    return false;
  }

  for (auto& value : frame.cfa_pattern) {
    value = static_cast<int>(read_i32_le(stream, ok));
  }
  for (auto& value : frame.black_levels) {
    value = static_cast<int>(read_u16_le(stream, ok));
  }
  for (auto& value : frame.white_levels) {
    value = static_cast<int>(read_u16_le(stream, ok));
  }
  if (!ok) {
    error_message = "PixelScope could not read the rawloader payload levels.";
    return false;
  }

  if (version >= 2) {
    frame.orientation = static_cast<int>(read_u32_le(stream, ok));
    for (auto& value : frame.wb_coeffs) {
      value = read_f32_le(stream, ok);
    }
    for (auto& value : frame.crops) {
      value = static_cast<int>(read_u32_le(stream, ok));
    }
    for (auto& value : frame.xyz_to_cam) {
      value = read_f32_le(stream, ok);
    }
    frame.make = read_string(stream, ok);
    frame.model = read_string(stream, ok);
    frame.clean_make = read_string(stream, ok);
    frame.clean_model = read_string(stream, ok);
    const auto metadata_entry_count = read_u32_le(stream, ok);
    if (!ok) {
      error_message = "PixelScope could not read the rawloader metadata block.";
      return false;
    }

    frame.metadata_entries.clear();
    frame.metadata_entries.reserve(static_cast<std::size_t>(metadata_entry_count));
    for (std::uint32_t index = 0; index < metadata_entry_count; ++index) {
      auto label = read_string(stream, ok);
      auto value = read_string(stream, ok);
      if (!ok) {
        error_message = "PixelScope could not read a rawloader metadata entry.";
        return false;
      }
      frame.metadata_entries.push_back({.label = std::move(label), .value = std::move(value)});
    }
  }

  const auto sample_count = read_u64_le(stream, ok);
  if (!ok) {
    error_message = "PixelScope could not read the rawloader payload sample count.";
    return false;
  }

  const std::size_t byte_count = static_cast<std::size_t>(sample_count) * sizeof(std::uint16_t);
  frame.decoded_bytes.resize(byte_count);
  if (!frame.decoded_bytes.empty()) {
    stream.read(reinterpret_cast<char*>(frame.decoded_bytes.data()), static_cast<std::streamsize>(byte_count));
    if (stream.gcount() != static_cast<std::streamsize>(byte_count)) {
      error_message = "PixelScope could not read the rawloader sample buffer.";
      return false;
    }
  }

  return true;
}

bool load_frame_with_rawloader(const std::string& input_path, DngFrame& frame, std::string& error_message) {
  const auto payload_path = temp_output_path(".bin");
  const auto stderr_path = temp_output_path(".log");
  ScopedFileCleanup payload_cleanup{payload_path};
  ScopedFileCleanup stderr_cleanup{stderr_path};
  const auto bridge_path = pixelscope::platform::resolve_companion_binary(
      PIXELSCOPE_RAWLOADER_BRIDGE_FILENAME,
      PIXELSCOPE_RAWLOADER_BRIDGE_PATH);

  if (!std::filesystem::exists(bridge_path)) {
    error_message = "rawloader bridge binary is missing from the build output.";
    return false;
  }

  if (!run_rawloader_bridge(input_path, payload_path, stderr_path, error_message)) {
    return false;
  }

  return load_frame_from_payload(payload_path, frame, error_message);
}

}  // namespace

pixelscope::core::ImageData make_raw_bayer_image(
    pixelscope::core::ImageMetadata metadata,
    std::vector<std::uint16_t> raw_samples,
    bool use_cfa_colors) {
  if (metadata.width <= 0 || metadata.height <= 0) {
    return {};
  }

  metadata.original_channel_count = 1;
  metadata.is_raw_bayer_plane = true;
  std::vector<std::uint8_t> rgba_pixels(static_cast<std::size_t>(metadata.width * metadata.height * 4), 255);
  pixelscope::core::ImageData image(std::move(metadata), std::move(rgba_pixels), std::move(raw_samples));
  return render_raw_bayer_image(image, use_cfa_colors);
}

pixelscope::core::ImageData rgba8_image_from_dng_frame(
    const DngFrame& frame,
    const std::string& source_path) {
  if (!is_supported_frame_layout(frame)) {
    return {};
  }

  std::vector<std::uint8_t> rgba_pixels(static_cast<std::size_t>(frame.width * frame.height * 4), 255);
  std::vector<std::uint16_t> raw_samples;
  std::vector<std::uint16_t> rgba16_pixels;
  const std::size_t pixel_count = static_cast<std::size_t>(frame.width) * static_cast<std::size_t>(frame.height);
  if (frame.samples_per_pixel == 1) {
    raw_samples.reserve(pixel_count);
  } else if ((frame.original_bits_per_sample > 8 || frame.bits_per_sample > 8)) {
    rgba16_pixels.resize(pixel_count * 4, std::numeric_limits<std::uint16_t>::max());
  }
  for (std::size_t pixel_index = 0; pixel_index < pixel_count; ++pixel_index) {
    const std::size_t source_base = pixel_index * static_cast<std::size_t>(frame.samples_per_pixel);
    const std::size_t output_base = pixel_index * 4;
    const int x = static_cast<int>(pixel_index % static_cast<std::size_t>(frame.width));
    const int y = static_cast<int>(pixel_index / static_cast<std::size_t>(frame.width));

    if (frame.samples_per_pixel == 1) {
      const auto raw_sample = static_cast<std::uint16_t>(read_frame_sample(frame, source_base));
      raw_samples.push_back(raw_sample);
      const auto value = raw_plane_to_u8(raw_sample, frame.original_bits_per_sample);
      const auto mosaic = cfa_mosaic_pixel(value, frame.cfa_pattern, x, y);
      rgba_pixels[output_base + 0] = mosaic.r;
      rgba_pixels[output_base + 1] = mosaic.g;
      rgba_pixels[output_base + 2] = mosaic.b;
      continue;
    }

    for (int channel = 0; channel < 3; ++channel) {
      const auto sample = read_frame_sample(frame, source_base + static_cast<std::size_t>(channel));
      rgba_pixels[output_base + static_cast<std::size_t>(channel)] = normalize_to_u8(
          sample,
          frame.black_levels[static_cast<std::size_t>(channel)],
          resolve_white_level(frame, channel));
      if (!rgba16_pixels.empty()) {
        rgba16_pixels[output_base + static_cast<std::size_t>(channel)] = static_cast<std::uint16_t>(sample);
      }
    }

    if (frame.samples_per_pixel == 4) {
      const auto alpha_sample = read_frame_sample(frame, source_base + 3);
      rgba_pixels[output_base + 3] = normalize_to_u8(
          alpha_sample,
          frame.black_levels[3],
          resolve_white_level(frame, 3));
      if (!rgba16_pixels.empty()) {
        rgba16_pixels[output_base + 3] = static_cast<std::uint16_t>(alpha_sample);
      }
    }
  }

  if (frame.samples_per_pixel == 1) {
    pixelscope::core::ImageMetadata metadata{
        .width = frame.width,
        .height = frame.height,
        .original_channel_count = 1,
        .bits_per_channel = frame.original_bits_per_sample > 0 ? frame.original_bits_per_sample : frame.bits_per_sample,
        .is_raw_bayer_plane = true,
        .cfa_pattern = frame.cfa_pattern,
        .metadata_entries = build_dng_metadata_entries(frame),
        .source_path = source_path,
    };
    return make_raw_bayer_image(std::move(metadata), std::move(raw_samples), true);
  }

  pixelscope::core::ImageMetadata metadata{
      .width = frame.width,
      .height = frame.height,
      .original_channel_count = frame.samples_per_pixel,
      .bits_per_channel = frame.original_bits_per_sample > 0 ? frame.original_bits_per_sample : frame.bits_per_sample,
      .is_raw_bayer_plane = false,
      .cfa_pattern = frame.cfa_pattern,
      .metadata_entries = build_dng_metadata_entries(frame),
      .source_path = source_path,
  };
  return pixelscope::core::ImageData(
      std::move(metadata),
      std::move(rgba_pixels),
      std::move(raw_samples),
      std::move(rgba16_pixels));
}

pixelscope::core::ImageData render_raw_bayer_image(
    const pixelscope::core::ImageData& source,
    bool use_cfa_colors) {
  if (!source.valid()) {
    return {};
  }

  const auto& metadata = source.metadata();
  const auto& raw_samples = source.raw_samples();
  if (!metadata.is_raw_bayer_plane || raw_samples.empty()) {
    return source;
  }

  auto [min_sample_it, max_sample_it] = std::minmax_element(raw_samples.begin(), raw_samples.end());
  const std::uint16_t observed_black = min_sample_it != raw_samples.end() ? *min_sample_it : 0;
  const std::uint16_t observed_white = max_sample_it != raw_samples.end() ? *max_sample_it : 0;

  std::vector<std::uint8_t> rgba_pixels(static_cast<std::size_t>(metadata.width * metadata.height * 4), 255);
  const std::size_t pixel_count = static_cast<std::size_t>(metadata.width) * static_cast<std::size_t>(metadata.height);
  for (std::size_t pixel_index = 0; pixel_index < pixel_count; ++pixel_index) {
    const std::size_t output_base = pixel_index * 4;
    const int x = static_cast<int>(pixel_index % static_cast<std::size_t>(metadata.width));
    const int y = static_cast<int>(pixel_index / static_cast<std::size_t>(metadata.width));
    const std::uint8_t value =
        normalize_raw_plane_to_u8(raw_samples[pixel_index], observed_black, observed_white);
    const auto pixel = use_cfa_colors
                           ? cfa_mosaic_pixel(value, metadata.cfa_pattern, x, y)
                           : pixelscope::core::PixelRgba8{.r = value, .g = value, .b = value, .a = 255};
    rgba_pixels[output_base + 0] = pixel.r;
    rgba_pixels[output_base + 1] = pixel.g;
    rgba_pixels[output_base + 2] = pixel.b;
    rgba_pixels[output_base + 3] = pixel.a;
  }

  return pixelscope::core::ImageData(metadata, std::move(rgba_pixels), raw_samples);
}

DngLoadResult load_dng_file(const std::string& path) {
  DngFrame frame;
  std::string error_message;
  if (!load_frame_with_rawloader(path, frame, error_message)) {
    if (error_message.empty()) {
      error_message = "Failed to decode DNG image with rawloader.";
    }
    return {.error_message = std::move(error_message)};
  }

  auto image = rgba8_image_from_dng_frame(frame, path);
  if (!image.valid()) {
    return {.error_message = "rawloader decode succeeded, but PixelScope could not convert the image to RGBA8."};
  }

  auto metadata = image.metadata();
  merge_metadata_entries(metadata.metadata_entries, load_embedded_metadata(path));
  image = pixelscope::core::ImageData(
      std::move(metadata),
      image.pixels_rgba8(),
      image.raw_samples(),
      image.pixels_rgba16());

  return {.image = std::move(image)};
}

}  // namespace pixelscope::io
