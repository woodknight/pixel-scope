#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "core/histogram.h"
#include "core/image.h"
#include "core/image_model.h"
#include "core/viewport.h"
#include "io/dng_loader.h"

int main() {
  {
    pixelscope::core::ImageMetadata metadata{
        .width = 2,
        .height = 1,
        .original_channel_count = 4,
        .bits_per_channel = 8,
        .source_path = "test.png",
    };
    pixelscope::core::ImageData image(metadata, std::vector<std::uint8_t>{
                                                    10, 20, 30, 255,
                                                    40, 50, 60, 255,
                                                });
    assert(image.valid());
    const auto pixel = image.pixel_at(1, 0);
    assert(pixel.has_value());
    assert(pixel->r == 40);
    assert(pixel->g == 50);
    assert(pixel->b == 60);
  }

  {
    const float zoom = pixelscope::core::fit_zoom(400, 200, 200.0f, 200.0f);
    assert(zoom == 0.5f);
  }

  {
    pixelscope::core::ViewState view{.zoom = 2.0f, .pan = {}};
    const pixelscope::core::Rect canvas{.x = 0.0f, .y = 0.0f, .w = 200.0f, .h = 200.0f};
    const auto image_point =
        pixelscope::core::screen_to_image({.x = 100.0f, .y = 100.0f}, 50, 50, canvas, view);
    assert(image_point.has_value());
    assert(static_cast<int>(image_point->x) == 25);
    assert(static_cast<int>(image_point->y) == 25);
  }

  {
    pixelscope::core::ViewState view{.zoom = 1.0f, .pan = {}};
    const pixelscope::core::Rect canvas{.x = 0.0f, .y = 0.0f, .w = 200.0f, .h = 200.0f};
    pixelscope::core::zoom_around_point(view, 2.0f, {.x = 100.0f, .y = 100.0f}, 50, 50, canvas);
    const auto anchored =
        pixelscope::core::screen_to_image({.x = 100.0f, .y = 100.0f}, 50, 50, canvas, view);
    assert(anchored.has_value());
    assert(static_cast<int>(anchored->x) == 25);
    assert(static_cast<int>(anchored->y) == 25);
  }

  {
    pixelscope::core::ImageMetadata metadata{
        .width = 2,
        .height = 2,
        .original_channel_count = 4,
        .bits_per_channel = 8,
        .source_path = "histogram.png",
    };
    pixelscope::core::ImageData image(metadata, std::vector<std::uint8_t>{
                                                    0, 0, 0, 255,
                                                    255, 0, 0, 255,
                                                    0, 255, 0, 255,
                                                    0, 0, 255, 255,
                                                });
    const auto histogram = pixelscope::core::compute_histogram(image);
    assert(!histogram.empty());
    assert(histogram.sample_count == 4);
    assert(histogram.red.bins[0] == 3);
    assert(histogram.red.bins[255] == 1);
    assert(histogram.green.bins[0] == 3);
    assert(histogram.green.bins[255] == 1);
    assert(histogram.blue.bins[0] == 3);
    assert(histogram.blue.bins[255] == 1);
    assert(histogram.luminance.bins[0] == 1);
    assert(histogram.luminance.bins[77] == 1);
    assert(histogram.luminance.bins[149] == 1);
    assert(histogram.luminance.bins[29] == 1);
  }

  {
    pixelscope::core::ImageMetadata metadata{
        .width = 513,
        .height = 9,
        .original_channel_count = 4,
        .bits_per_channel = 8,
        .source_path = "histogram-large.png",
    };
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(metadata.width * metadata.height * 4));
    for (int y = 0; y < metadata.height; ++y) {
      for (int x = 0; x < metadata.width; ++x) {
        const std::size_t index = static_cast<std::size_t>((y * metadata.width + x) * 4);
        pixels[index + 0] = static_cast<std::uint8_t>(x % 256);
        pixels[index + 1] = static_cast<std::uint8_t>((x + y) % 256);
        pixels[index + 2] = static_cast<std::uint8_t>((x * 3 + y * 5) % 256);
        pixels[index + 3] = static_cast<std::uint8_t>(255 - (x % 64));
      }
    }

    const pixelscope::core::ImageData image(metadata, std::move(pixels));
    pixelscope::core::ImageHistogram expected;
    expected.sample_count = static_cast<std::size_t>(metadata.width * metadata.height);
    const auto& source_pixels = image.pixels_rgba8();
    for (std::size_t index = 0; index + 3 < source_pixels.size(); index += 4) {
      const auto red = source_pixels[index + 0];
      const auto green = source_pixels[index + 1];
      const auto blue = source_pixels[index + 2];
      ++expected.red.bins[red];
      ++expected.green.bins[green];
      ++expected.blue.bins[blue];
      const int weighted_sum = 77 * static_cast<int>(red) + 150 * static_cast<int>(green) +
                               29 * static_cast<int>(blue) + 128;
      ++expected.luminance.bins[static_cast<std::size_t>(weighted_sum / 256)];
    }
    for (std::size_t index = 0; index < 256; ++index) {
      expected.red.max_count = std::max(expected.red.max_count, expected.red.bins[index]);
      expected.green.max_count = std::max(expected.green.max_count, expected.green.bins[index]);
      expected.blue.max_count = std::max(expected.blue.max_count, expected.blue.bins[index]);
      expected.luminance.max_count = std::max(expected.luminance.max_count, expected.luminance.bins[index]);
    }

    const auto histogram = pixelscope::core::compute_histogram(image);
    assert(histogram.sample_count == expected.sample_count);
    assert(histogram.red.bins == expected.red.bins);
    assert(histogram.green.bins == expected.green.bins);
    assert(histogram.blue.bins == expected.blue.bins);
    assert(histogram.luminance.bins == expected.luminance.bins);
    assert(histogram.red.max_count == expected.red.max_count);
    assert(histogram.green.max_count == expected.green.max_count);
    assert(histogram.blue.max_count == expected.blue.max_count);
    assert(histogram.luminance.max_count == expected.luminance.max_count);
  }

  {
    pixelscope::core::ImageMetadata metadata{
        .width = 4,
        .height = 2,
        .original_channel_count = 4,
        .bits_per_channel = 8,
        .source_path = "downsample.png",
    };
    pixelscope::core::ImageData image(metadata, std::vector<std::uint8_t>{
                                                    1, 0, 0, 255,
                                                    2, 0, 0, 255,
                                                    3, 0, 0, 255,
                                                    4, 0, 0, 255,
                                                    5, 0, 0, 255,
                                                    6, 0, 0, 255,
                                                    7, 0, 0, 255,
                                                    8, 0, 0, 255,
                                                });
    const auto downsampled = pixelscope::core::downsample_nearest_2x(image);
    assert(downsampled.valid());
    assert(downsampled.metadata().width == 2);
    assert(downsampled.metadata().height == 1);
    assert(downsampled.pixel_at(0, 0)->r == 1);
    assert(downsampled.pixel_at(1, 0)->r == 3);
  }

  {
    pixelscope::core::ImageMetadata metadata{
        .width = 16,
        .height = 8,
        .original_channel_count = 4,
        .bits_per_channel = 8,
        .source_path = "model.png",
    };
    pixelscope::core::ImageData image(
        metadata,
        std::vector<std::uint8_t>(static_cast<std::size_t>(16 * 8 * 4), 255));
    const auto model = pixelscope::core::build_image_model(std::move(image), 4);
    assert(model.valid());
    assert(model.display_levels.size() == 2);
    assert(model.display_levels[0].downsample_factor == 2);
    assert(model.display_levels[0].image.metadata().width == 8);
    assert(model.display_levels[1].downsample_factor == 4);
    assert(model.display_levels[1].image.metadata().width == 4);
    assert(model.pick_display_level(1.0f) == nullptr);
    assert(model.pick_display_level(2.0f) == nullptr);
    assert(model.pick_display_level(0.6f)->downsample_factor == 2);
    assert(model.pick_display_level(0.3f)->downsample_factor == 4);
    assert(model.pick_display_level(0.1f)->downsample_factor == 4);
  }

  {
    pixelscope::io::DngFrame frame{
        .width = 2,
        .height = 2,
        .samples_per_pixel = 1,
        .bits_per_sample = 16,
        .original_bits_per_sample = 10,
        .cfa_pattern = {0, 1, 1, 2},
        .black_levels = {0, 0, 0, 0},
        .white_levels = {-1, -1, -1, -1},
        .decoded_bytes =
            {
                64, 0,
                255, 3,
                128, 1,
                0, 1,
            },
    };
    const auto image = pixelscope::io::rgba8_image_from_dng_frame(frame, "mono.dng");
    assert(image.valid());
    assert(image.metadata().bits_per_channel == 10);
    assert(image.metadata().is_raw_bayer_plane);
    assert(image.metadata().cfa_pattern[0] == 0);
    assert(image.metadata().cfa_pattern[1] == 1);
    assert(image.metadata().cfa_pattern[2] == 1);
    assert(image.metadata().cfa_pattern[3] == 2);
    assert(image.has_raw_samples());
    assert(image.raw_sample_at(0, 0).value() == 64);
    assert(image.raw_sample_at(1, 0).value() == 1023);
    assert(image.raw_sample_at(0, 1).value() == 384);
    assert(image.raw_sample_at(1, 1).value() == 256);
    assert(image.pixel_at(0, 0)->r == 16);
    assert(image.pixel_at(0, 0)->g == 16);
    assert(image.pixel_at(0, 0)->b == 16);
    assert(image.pixel_at(1, 0)->r == 255);
    assert(image.pixel_at(1, 0)->g == 255);
    assert(image.pixel_at(1, 0)->b == 255);
    assert(image.pixel_at(0, 1)->r == 96);
    assert(image.pixel_at(0, 1)->g == 96);
    assert(image.pixel_at(0, 1)->b == 96);
    assert(image.pixel_at(1, 1)->r == 64);
    assert(image.pixel_at(1, 1)->g == 64);
    assert(image.pixel_at(1, 1)->b == 64);
  }

  {
    pixelscope::io::DngFrame frame{
        .width = 1,
        .height = 1,
        .samples_per_pixel = 3,
        .bits_per_sample = 16,
        .original_bits_per_sample = 10,
        .black_levels = {0, 128, 64, 0},
        .white_levels = {1023, 1151, 1087, -1},
        .decoded_bytes =
            {
                0, 2,
                128, 2,
                64, 2,
            },
    };
    const auto image = pixelscope::io::rgba8_image_from_dng_frame(frame, "rgb.dng");
    assert(image.valid());
    assert(!image.metadata().is_raw_bayer_plane);
    assert(!image.has_raw_samples());
    const auto pixel = image.pixel_at(0, 0);
    assert(pixel.has_value());
    assert(pixel->r == 128);
    assert(pixel->g == 128);
    assert(pixel->b == 128);
    assert(pixel->a == 255);
  }

  return 0;
}
