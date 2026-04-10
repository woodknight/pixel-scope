#pragma once

#include <array>
#include <string>

#include "core/image.h"

namespace pixelscope::io {

struct BinaryRawParameters {
  int width = 0;
  int height = 0;
  int bits_per_sample = 0;
  bool little_endian = true;
  std::array<int, 4> cfa_pattern = {0, 1, 1, 2};
};

struct BinaryRawParameterGuess {
  BinaryRawParameters parameters;
  bool width_guessed = false;
  bool height_guessed = false;
  bool bits_guessed = false;
  bool endianness_guessed = false;
  bool cfa_guessed = false;
};

struct BinaryRawLoadResult {
  pixelscope::core::ImageData image;
  std::string error_message;

  [[nodiscard]] bool ok() const { return error_message.empty() && image.valid(); }
};

[[nodiscard]] bool is_binary_raw_file_path(const std::string& path);
[[nodiscard]] BinaryRawParameterGuess guess_binary_raw_parameters_from_filename(const std::string& path);
[[nodiscard]] BinaryRawLoadResult load_binary_raw_file(const std::string& path, const BinaryRawParameters& parameters);

}  // namespace pixelscope::io
