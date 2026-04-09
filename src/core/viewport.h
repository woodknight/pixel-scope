#pragma once

#include <optional>

namespace pixelscope::core {

struct Vec2 {
  float x = 0.0f;
  float y = 0.0f;
};

struct Rect {
  float x = 0.0f;
  float y = 0.0f;
  float w = 0.0f;
  float h = 0.0f;
};

struct ViewState {
  float zoom = 1.0f;
  Vec2 pan = {};
};

[[nodiscard]] float fit_zoom(int image_width, int image_height, float canvas_width, float canvas_height);
[[nodiscard]] Rect image_rect(int image_width, int image_height, const Rect& canvas_rect, const ViewState& view);
[[nodiscard]] std::optional<Vec2> screen_to_image(
    const Vec2& screen_point,
    int image_width,
    int image_height,
    const Rect& canvas_rect,
    const ViewState& view);
void zoom_around_point(
    ViewState& view,
    float next_zoom,
    const Vec2& anchor_screen_point,
    int image_width,
    int image_height,
    const Rect& canvas_rect);

}  // namespace pixelscope::core
