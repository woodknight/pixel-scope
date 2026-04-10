#include "io/binary_raw_loader.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/image.h"
#include "io/dng_loader.h"

namespace pixelscope::io {

namespace {

struct ParsedFilenameTokens {
  std::vector<int> numbers;
  std::string lowered_name;
};

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

bool contains_token(const std::string& haystack, std::string_view token) {
  return haystack.find(token) != std::string::npos;
}

ParsedFilenameTokens parse_filename_tokens(const std::string& path) {
  ParsedFilenameTokens tokens;
  tokens.lowered_name = std::filesystem::path(path).filename().string();
  std::transform(tokens.lowered_name.begin(), tokens.lowered_name.end(), tokens.lowered_name.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });

  int current_value = 0;
  bool collecting = false;
  for (char c : tokens.lowered_name) {
    if (std::isdigit(static_cast<unsigned char>(c)) != 0) {
      collecting = true;
      current_value = (current_value * 10) + (c - '0');
      continue;
    }
    if (collecting) {
      tokens.numbers.push_back(current_value);
      current_value = 0;
      collecting = false;
    }
  }
  if (collecting) {
    tokens.numbers.push_back(current_value);
  }

  return tokens;
}

std::optional<std::array<int, 4>> parse_cfa_pattern(const std::string& name) {
  if (contains_token(name, "rggb")) {
    return std::array<int, 4>{0, 1, 1, 2};
  }
  if (contains_token(name, "bggr")) {
    return std::array<int, 4>{2, 1, 1, 0};
  }
  if (contains_token(name, "grbg")) {
    return std::array<int, 4>{1, 0, 2, 1};
  }
  if (contains_token(name, "gbrg")) {
    return std::array<int, 4>{1, 2, 0, 1};
  }
  return std::nullopt;
}

std::optional<int> parse_bits_per_sample(const std::string& name, const std::vector<int>& numbers) {
  for (const int candidate : {8, 16}) {
    const std::string needle = std::to_string(candidate) + "bit";
    if (contains_token(name, needle) || contains_token(name, std::to_string(candidate) + "bpp")) {
      return candidate;
    }
  }

  for (const int value : numbers) {
    if (value == 8 || value == 16) {
      return value;
    }
  }
  return std::nullopt;
}

std::optional<bool> parse_endianness(const std::string& name) {
  if (contains_token(name, "little") || contains_token(name, "_le") || contains_token(name, "-le") ||
      contains_token(name, ".le")) {
    return true;
  }
  if (contains_token(name, "big") || contains_token(name, "_be") || contains_token(name, "-be") ||
      contains_token(name, ".be")) {
    return false;
  }
  return std::nullopt;
}

std::pair<int, int> guess_dimensions(const std::vector<int>& numbers) {
  int width = 0;
  int height = 0;
  for (std::size_t index = 0; index + 1 < numbers.size(); ++index) {
    const int first = numbers[index];
    const int second = numbers[index + 1];
    if (first >= 16 && second >= 16) {
      width = first;
      height = second;
      break;
    }
  }

  if (width <= 0 || height <= 0) {
    std::vector<int> dimension_candidates;
    for (const int value : numbers) {
      if (value >= 16) {
        dimension_candidates.push_back(value);
      }
    }
    if (dimension_candidates.size() >= 2) {
      width = dimension_candidates[0];
      height = dimension_candidates[1];
    }
  }

  return {width, height};
}

bool validate_parameters(const BinaryRawParameters& parameters, std::string& error_message) {
  if (parameters.width <= 0 || parameters.height <= 0) {
    error_message = "Binary raw width and height must be greater than zero.";
    return false;
  }
  if (parameters.bits_per_sample != 8 && parameters.bits_per_sample != 16) {
    error_message = "Binary raw bit width must be either 8 or 16.";
    return false;
  }
  for (const int channel : parameters.cfa_pattern) {
    if (channel < 0 || channel > 2) {
      error_message = "Binary raw CFA pattern must be one of RGGB, BGGR, GRBG, or GBRG.";
      return false;
    }
  }
  return true;
}

std::uint16_t read_unpacked_sample(
    const std::vector<std::uint8_t>& bytes,
    std::size_t sample_index,
    int bytes_per_sample,
    bool little_endian) {
  const std::size_t offset = sample_index * static_cast<std::size_t>(bytes_per_sample);
  if (bytes_per_sample == 1) {
    return bytes[offset];
  }

  const std::uint16_t first = bytes[offset];
  const std::uint16_t second = bytes[offset + 1];
  return little_endian ? static_cast<std::uint16_t>(first | (second << 8U))
                       : static_cast<std::uint16_t>((first << 8U) | second);
}

std::vector<std::uint16_t> unpack_samples(
    const std::vector<std::uint8_t>& bytes,
    std::size_t pixel_count,
    int bits_per_sample,
    bool little_endian,
    std::string& error_message) {
  std::vector<std::uint16_t> samples;
  samples.reserve(pixel_count);

  const int bytes_per_sample = bits_per_sample / 8;
  const std::size_t expected_bytes = pixel_count * static_cast<std::size_t>(bytes_per_sample);
  if (bytes.size() != expected_bytes) {
    error_message = "Binary raw file size does not match width, height, and bit width.";
    return {};
  }

  for (std::size_t sample_index = 0; sample_index < pixel_count; ++sample_index) {
    samples.push_back(read_unpacked_sample(bytes, sample_index, bytes_per_sample, little_endian));
  }

  return samples;
}

}  // namespace

bool is_binary_raw_file_path(const std::string& path) {
  const std::string extension = normalized_extension(path);
  return extension == ".raw" || extension == ".bin" || extension == ".bayer";
}

BinaryRawParameterGuess guess_binary_raw_parameters_from_filename(const std::string& path) {
  BinaryRawParameterGuess guess;
  const ParsedFilenameTokens tokens = parse_filename_tokens(path);

  const auto [width, height] = guess_dimensions(tokens.numbers);
  guess.parameters.width = width;
  guess.parameters.height = height;
  guess.width_guessed = width > 0;
  guess.height_guessed = height > 0;

  if (const auto cfa = parse_cfa_pattern(tokens.lowered_name)) {
    guess.parameters.cfa_pattern = *cfa;
    guess.cfa_guessed = true;
  }

  if (const auto bits = parse_bits_per_sample(tokens.lowered_name, tokens.numbers)) {
    guess.parameters.bits_per_sample = *bits;
    guess.bits_guessed = true;
  } else {
    guess.parameters.bits_per_sample = 16;
  }

  if (const auto little_endian = parse_endianness(tokens.lowered_name)) {
    guess.parameters.little_endian = *little_endian;
    guess.endianness_guessed = true;
  }

  return guess;
}

BinaryRawLoadResult load_binary_raw_file(const std::string& path, const BinaryRawParameters& parameters) {
  std::string error_message;
  if (!validate_parameters(parameters, error_message)) {
    return {.error_message = std::move(error_message)};
  }

  const std::size_t pixel_count = static_cast<std::size_t>(parameters.width) * static_cast<std::size_t>(parameters.height);
  if (pixel_count > (std::numeric_limits<std::size_t>::max() / sizeof(std::uint16_t))) {
    return {.error_message = "Binary raw dimensions are too large."};
  }

  std::ifstream stream(path, std::ios::binary);
  if (!stream.is_open()) {
    return {.error_message = "PixelScope could not open the binary raw file."};
  }

  stream.seekg(0, std::ios::end);
  const std::streamoff stream_size = stream.tellg();
  if (stream_size < 0) {
    return {.error_message = "PixelScope could not determine the binary raw file size."};
  }
  stream.seekg(0, std::ios::beg);

  std::vector<std::uint8_t> bytes(static_cast<std::size_t>(stream_size));
  if (!bytes.empty()) {
    stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (stream.gcount() != static_cast<std::streamsize>(bytes.size())) {
      return {.error_message = "PixelScope could not read the binary raw file contents."};
    }
  }

  std::vector<std::uint16_t> raw_samples =
      unpack_samples(bytes, pixel_count, parameters.bits_per_sample, parameters.little_endian, error_message);
  if (!error_message.empty()) {
    return {.error_message = std::move(error_message)};
  }

  pixelscope::core::ImageMetadata metadata{
      .width = parameters.width,
      .height = parameters.height,
      .original_channel_count = 1,
      .bits_per_channel = parameters.bits_per_sample,
      .is_raw_bayer_plane = true,
      .cfa_pattern = parameters.cfa_pattern,
      .source_path = path,
  };
  return {.image = make_raw_bayer_image(std::move(metadata), std::move(raw_samples), true)};
}

}  // namespace pixelscope::io
