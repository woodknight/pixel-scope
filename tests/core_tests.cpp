#include <cassert>
#include <cstdint>
#include <vector>

#include "core/histogram.h"
#include "core/image.h"
#include "core/viewport.h"

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

  return 0;
}
