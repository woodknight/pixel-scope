#include "core/image_statistics.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <thread>
#include <vector>

namespace pixelscope::core {

namespace {

constexpr std::size_t kPixelStride = 4;
constexpr std::size_t kParallelThresholdPixels = 1U << 20;
constexpr std::size_t kMaxValueBins = static_cast<std::size_t>(std::numeric_limits<std::uint16_t>::max()) + 1U;

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

struct StatisticsAccumulator {
  std::vector<std::uint32_t> bins;
  std::uint64_t sum = 0;
  std::uint32_t min_value = std::numeric_limits<std::uint32_t>::max();
  std::uint32_t max_value = 0;
};

std::uint16_t rgba8_luminance_at(const std::vector<std::uint8_t>& pixels, std::size_t pixel_index) {
  const auto& red_lut = red_luminance_lut();
  const auto& green_lut = green_luminance_lut();
  const std::size_t index = pixel_index * kPixelStride;
  return static_cast<std::uint16_t>(
      (red_lut[pixels[index + 0]] + green_lut[pixels[index + 1]] + (29U * pixels[index + 2]) + 128U) >> 8U);
}

std::uint16_t rgba16_luminance_at(const std::vector<std::uint16_t>& pixels, std::size_t pixel_index) {
  const std::size_t index = pixel_index * kPixelStride;
  const std::uint32_t weighted_sum = 77U * pixels[index + 0] + 150U * pixels[index + 1] + 29U * pixels[index + 2] +
                                     128U;
  return static_cast<std::uint16_t>(weighted_sum >> 8U);
}

template <typename ValueFn>
void accumulate_range(
    StatisticsAccumulator& accumulator,
    std::size_t begin_pixel,
    std::size_t end_pixel,
    const ValueFn& value_at) {
  for (std::size_t pixel_index = begin_pixel; pixel_index < end_pixel; ++pixel_index) {
    const std::uint32_t value = value_at(pixel_index);
    ++accumulator.bins[value];
    accumulator.sum += value;
    accumulator.min_value = std::min(accumulator.min_value, value);
    accumulator.max_value = std::max(accumulator.max_value, value);
  }
}

void merge_accumulator(StatisticsAccumulator& target, const StatisticsAccumulator& source) {
  for (std::size_t index = 0; index < target.bins.size(); ++index) {
    target.bins[index] += source.bins[index];
  }
  target.sum += source.sum;
  target.min_value = std::min(target.min_value, source.min_value);
  target.max_value = std::max(target.max_value, source.max_value);
}

std::uint32_t percentile_from_histogram(
    const std::vector<std::uint32_t>& bins,
    std::size_t sample_count,
    double percentile) {
  if (bins.empty() || sample_count == 0) {
    return 0;
  }

  const double clamped = std::clamp(percentile, 0.0, 1.0);
  const std::size_t target_rank =
      static_cast<std::size_t>(clamped * static_cast<double>(sample_count - 1)) + 1U;
  std::size_t cumulative = 0;
  for (std::size_t index = 0; index < bins.size(); ++index) {
    cumulative += bins[index];
    if (cumulative >= target_rank) {
      return static_cast<std::uint32_t>(index);
    }
  }

  return static_cast<std::uint32_t>(bins.size() - 1);
}

template <typename ValueFn>
ImageStatistics compute_statistics_impl(
    std::size_t pixel_count,
    bool uses_raw_samples,
    bool uses_high_precision_luminance,
    const ValueFn& value_at) {
  if (pixel_count == 0) {
    return {};
  }

  const unsigned int worker_limit = std::max(1U, std::thread::hardware_concurrency());
  const std::size_t worker_count =
      pixel_count >= kParallelThresholdPixels ? std::min<std::size_t>(worker_limit, pixel_count) : 1U;

  if (worker_count == 1U) {
    StatisticsAccumulator accumulator{.bins = std::vector<std::uint32_t>(kMaxValueBins, 0U)};
    accumulate_range(accumulator, 0, pixel_count, value_at);
    ImageStatistics statistics;
    statistics.sample_count = pixel_count;
    statistics.uses_raw_samples = uses_raw_samples;
    statistics.uses_high_precision_luminance = uses_high_precision_luminance;
    statistics.min_value = accumulator.min_value;
    statistics.max_value = accumulator.max_value;
    statistics.mean = static_cast<double>(accumulator.sum) / static_cast<double>(pixel_count);
    statistics.percentile_10 = percentile_from_histogram(accumulator.bins, pixel_count, 0.10);
    statistics.median = percentile_from_histogram(accumulator.bins, pixel_count, 0.50);
    statistics.percentile_90 = percentile_from_histogram(accumulator.bins, pixel_count, 0.90);
    return statistics;
  }

  std::vector<StatisticsAccumulator> accumulators;
  accumulators.reserve(worker_count);
  for (std::size_t worker_index = 0; worker_index < worker_count; ++worker_index) {
    accumulators.push_back(StatisticsAccumulator{.bins = std::vector<std::uint32_t>(kMaxValueBins, 0U)});
  }

  std::vector<std::thread> workers;
  workers.reserve(worker_count - 1);

  const std::size_t chunk_size = pixel_count / worker_count;
  std::size_t begin_pixel = 0;
  for (std::size_t worker_index = 0; worker_index < worker_count; ++worker_index) {
    const std::size_t end_pixel = worker_index + 1 == worker_count ? pixel_count : begin_pixel + chunk_size;
    if (worker_index + 1 == worker_count) {
      accumulate_range(accumulators[worker_index], begin_pixel, end_pixel, value_at);
    } else {
      workers.emplace_back(
          [&, worker_index, begin_pixel, end_pixel] {
            accumulate_range(accumulators[worker_index], begin_pixel, end_pixel, value_at);
          });
    }
    begin_pixel = end_pixel;
  }

  for (auto& worker : workers) {
    worker.join();
  }

  StatisticsAccumulator merged{.bins = std::vector<std::uint32_t>(kMaxValueBins, 0U)};
  for (const auto& accumulator : accumulators) {
    merge_accumulator(merged, accumulator);
  }

  ImageStatistics statistics;
  statistics.sample_count = pixel_count;
  statistics.uses_raw_samples = uses_raw_samples;
  statistics.uses_high_precision_luminance = uses_high_precision_luminance;
  statistics.min_value = merged.min_value;
  statistics.max_value = merged.max_value;
  statistics.mean = static_cast<double>(merged.sum) / static_cast<double>(pixel_count);
  statistics.percentile_10 = percentile_from_histogram(merged.bins, pixel_count, 0.10);
  statistics.median = percentile_from_histogram(merged.bins, pixel_count, 0.50);
  statistics.percentile_90 = percentile_from_histogram(merged.bins, pixel_count, 0.90);
  return statistics;
}

}  // namespace

ImageStatistics compute_image_statistics(const ImageData& image) {
  if (!image.valid()) {
    return {};
  }

  const std::size_t pixel_count = static_cast<std::size_t>(image.metadata().width * image.metadata().height);
  if (image.metadata().is_raw_bayer_plane && image.has_raw_samples()) {
    const auto& raw_samples = image.raw_samples();
    return compute_statistics_impl(pixel_count, true, false, [&](std::size_t pixel_index) {
      return static_cast<std::uint32_t>(raw_samples[pixel_index]);
    });
  }

  if (image.has_pixels_rgba16()) {
    const auto& pixels16 = image.pixels_rgba16();
    return compute_statistics_impl(pixel_count, false, true, [&](std::size_t pixel_index) {
      return static_cast<std::uint32_t>(rgba16_luminance_at(pixels16, pixel_index));
    });
  }

  const auto& pixels8 = image.pixels_rgba8();
  return compute_statistics_impl(pixel_count, false, false, [&](std::size_t pixel_index) {
    return static_cast<std::uint32_t>(rgba8_luminance_at(pixels8, pixel_index));
  });
}

}  // namespace pixelscope::core
