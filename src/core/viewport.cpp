#include "core/viewport.h"

#include <algorithm>

namespace pixelscope::core {

namespace {

constexpr float kMinZoom = 0.05f;

Vec2 canvas_center(const Rect& rect) {
  return Vec2{rect.x + rect.w * 0.5f, rect.y + rect.h * 0.5f};
}

}  // namespace

float fit_zoom(int image_width, int image_height, float canvas_width, float canvas_height) {
  if (image_width <= 0 || image_height <= 0 || canvas_width <= 0.0f || canvas_height <= 0.0f) {
    return 1.0f;
  }

  const float scale_x = canvas_width / static_cast<float>(image_width);
  const float scale_y = canvas_height / static_cast<float>(image_height);
  return std::max(kMinZoom, std::min(scale_x, scale_y));
}

Rect image_rect(int image_width, int image_height, const Rect& canvas_rect, const ViewState& view) {
  const Vec2 center = canvas_center(canvas_rect);
  const float scaled_w = static_cast<float>(image_width) * view.zoom;
  const float scaled_h = static_cast<float>(image_height) * view.zoom;
  return Rect{
      .x = center.x + view.pan.x - scaled_w * 0.5f,
      .y = center.y + view.pan.y - scaled_h * 0.5f,
      .w = scaled_w,
      .h = scaled_h,
  };
}

std::optional<Vec2> screen_to_image(
    const Vec2& screen_point,
    int image_width,
    int image_height,
    const Rect& canvas_rect,
    const ViewState& view) {
  if (image_width <= 0 || image_height <= 0 || view.zoom <= 0.0f) {
    return std::nullopt;
  }

  const Rect image_bounds = image_rect(image_width, image_height, canvas_rect, view);
  if (screen_point.x < image_bounds.x || screen_point.y < image_bounds.y ||
      screen_point.x >= image_bounds.x + image_bounds.w || screen_point.y >= image_bounds.y + image_bounds.h) {
    return std::nullopt;
  }

  return Vec2{
      .x = (screen_point.x - image_bounds.x) / view.zoom,
      .y = (screen_point.y - image_bounds.y) / view.zoom,
  };
}

void zoom_around_point(
    ViewState& view,
    float next_zoom,
    const Vec2& anchor_screen_point,
    int image_width,
    int image_height,
    const Rect& canvas_rect) {
  if (image_width <= 0 || image_height <= 0) {
    view.zoom = std::max(kMinZoom, next_zoom);
    return;
  }

  const auto image_point = screen_to_image(anchor_screen_point, image_width, image_height, canvas_rect, view);
  const float clamped_zoom = std::max(kMinZoom, next_zoom);
  if (!image_point) {
    view.zoom = clamped_zoom;
    return;
  }

  view.zoom = clamped_zoom;
  const Vec2 center = canvas_center(canvas_rect);
  const float scaled_x = image_point->x * view.zoom;
  const float scaled_y = image_point->y * view.zoom;
  const float scaled_w = static_cast<float>(image_width) * view.zoom;
  const float scaled_h = static_cast<float>(image_height) * view.zoom;
  view.pan.x = anchor_screen_point.x - center.x - scaled_x + scaled_w * 0.5f;
  view.pan.y = anchor_screen_point.y - center.y - scaled_y + scaled_h * 0.5f;
}

}  // namespace pixelscope::core
