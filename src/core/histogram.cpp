#include "core/histogram.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <thread>
#include <vector>

namespace pixelscope::core {

namespace {

constexpr std::size_t kPixelStride = 4;
constexpr std::size_t kParallelThresholdPixels = 1U << 20;

struct HistogramAccumulator {
  std::array<std::uint32_t, 256> red = {};
  std::array<std::uint32_t, 256> green = {};
  std::array<std::uint32_t, 256> blue = {};
  std::array<std::uint32_t, 256> luminance = {};
};

const std::array<std::uint16_t, 256>& red_luminance_lut() {
  static const auto lut = [] {
    std::array<std::uint16_t, 256> values = {};
    for (std::size_t index = 0; index < values.size(); ++index) {
      values[index] = static_cast<std::uint16_t>(77U * index);
    }
    return values;
  }();
  return lut;
}

const std::array<std::uint16_t, 256>& green_luminance_lut() {
  static const auto lut = [] {
    std::array<std::uint16_t, 256> values = {};
    for (std::size_t index = 0; index < values.size(); ++index) {
      values[index] = static_cast<std::uint16_t>(150U * index);
    }
    return values;
  }();
  return lut;
}

const std::array<std::uint16_t, 256>& blue_luminance_lut() {
  static const auto lut = [] {
    std::array<std::uint16_t, 256> values = {};
    for (std::size_t index = 0; index < values.size(); ++index) {
      values[index] = static_cast<std::uint16_t>(29U * index);
    }
    return values;
  }();
  return lut;
}

void accumulate_range(
    HistogramAccumulator& accumulator,
    const std::vector<std::uint8_t>& pixels,
    std::size_t begin_pixel,
    std::size_t end_pixel) {
  const auto& red_lut = red_luminance_lut();
  const auto& green_lut = green_luminance_lut();
  const auto& blue_lut = blue_luminance_lut();

  std::size_t index = begin_pixel * kPixelStride;
  const std::size_t end_index = end_pixel * kPixelStride;
  while (index < end_index) {
    const std::uint8_t red = pixels[index + 0];
    const std::uint8_t green = pixels[index + 1];
    const std::uint8_t blue = pixels[index + 2];
    ++accumulator.red[red];
    ++accumulator.green[green];
    ++accumulator.blue[blue];

    const std::uint16_t weighted_sum = red_lut[red] + green_lut[green] + blue_lut[blue] + 128U;
    ++accumulator.luminance[weighted_sum >> 8U];
    index += kPixelStride;
  }
}

void merge_accumulator(HistogramAccumulator& target, const HistogramAccumulator& source) {
  for (std::size_t index = 0; index < 256; ++index) {
    target.red[index] += source.red[index];
    target.green[index] += source.green[index];
    target.blue[index] += source.blue[index];
    target.luminance[index] += source.luminance[index];
  }
}

void finalize_channel(HistogramChannel& channel, const std::array<std::uint32_t, 256>& bins) {
  channel.bins = bins;
  channel.max_count = *std::max_element(channel.bins.begin(), channel.bins.end());
}

ImageHistogram make_histogram(const HistogramAccumulator& accumulator, std::size_t sample_count) {
  ImageHistogram histogram;
  histogram.sample_count = sample_count;
  finalize_channel(histogram.red, accumulator.red);
  finalize_channel(histogram.green, accumulator.green);
  finalize_channel(histogram.blue, accumulator.blue);
  finalize_channel(histogram.luminance, accumulator.luminance);
  return histogram;
}

}  // namespace

ImageHistogram compute_histogram(const ImageData& image) {
  if (!image.valid()) {
    return {};
  }

  const auto& pixels = image.pixels_rgba8();
  const std::size_t pixel_count = static_cast<std::size_t>(image.metadata().width * image.metadata().height);
  const unsigned int worker_limit = std::max(1U, std::thread::hardware_concurrency());
  const std::size_t worker_count =
      pixel_count >= kParallelThresholdPixels ? std::min<std::size_t>(worker_limit, pixel_count) : 1U;

  if (worker_count == 1U) {
    HistogramAccumulator accumulator;
    accumulate_range(accumulator, pixels, 0, pixel_count);
    return make_histogram(accumulator, pixel_count);
  }

  std::vector<HistogramAccumulator> accumulators(worker_count);
  std::vector<std::thread> workers;
  workers.reserve(worker_count - 1);

  const std::size_t chunk_size = pixel_count / worker_count;
  std::size_t begin_pixel = 0;
  for (std::size_t worker_index = 0; worker_index < worker_count; ++worker_index) {
    const std::size_t end_pixel =
        worker_index + 1 == worker_count ? pixel_count : begin_pixel + chunk_size;
    if (worker_index + 1 == worker_count) {
      accumulate_range(accumulators[worker_index], pixels, begin_pixel, end_pixel);
    } else {
      workers.emplace_back(
          [&, worker_index, begin_pixel, end_pixel] {
            accumulate_range(accumulators[worker_index], pixels, begin_pixel, end_pixel);
          });
    }
    begin_pixel = end_pixel;
  }

  for (auto& worker : workers) {
    worker.join();
  }

  HistogramAccumulator merged;
  for (const auto& accumulator : accumulators) {
    merge_accumulator(merged, accumulator);
  }
  return make_histogram(merged, pixel_count);
}

}  // namespace pixelscope::core
