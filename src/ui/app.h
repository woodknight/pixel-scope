#pragma once

#include <SDL.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <imgui.h>

#include "core/histogram.h"
#include "core/image_model.h"
#include "core/image_statistics.h"
#include "core/viewport.h"
#include "render/texture_cache.h"

namespace pixelscope::ui {

class App {
 public:
  explicit App(std::optional<std::string> initial_path);
  ~App();

  [[nodiscard]] bool initialize();
  int run();

 private:
  struct HoverState {
    int x = -1;
    int y = -1;
    pixelscope::core::PixelRgba8 pixel = {};
    std::optional<pixelscope::core::PixelRgba16> pixel16;
    std::optional<std::uint16_t> raw_sample;
    bool active = false;
  };

  bool load_image(const std::string& path);
  void fit_image_to_canvas(float width, float height);
  [[nodiscard]] float compute_renderer_scale() const;
  [[nodiscard]] float compute_ui_scale() const;
  [[nodiscard]] std::vector<std::string> preferred_font_paths() const;
  [[nodiscard]] bool create_renderer();
  void update_renderer_scale();
  void apply_ui_scale(float scale);
  void update_ui_scale_if_needed();
  void process_event(const SDL_Event& event, bool& running, bool& request_open_dialog);
  void draw_ui(bool& request_open_dialog);
  void draw_menu(bool& request_open_dialog);
  void draw_canvas();
  void draw_hover_overlay(const ImVec2& canvas_pos);
  void draw_histogram_overlay(const pixelscope::core::Rect& canvas_rect);
  void draw_statistics_overlay(const pixelscope::core::Rect& canvas_rect);
  void draw_status_bar();
  void refresh_dng_rendering();
  void reset_hover();
  void maybe_enable_pixel_grid_for_zoom();

  std::optional<std::string> initial_path_;
  SDL_Window* window_ = nullptr;
  SDL_Renderer* renderer_ = nullptr;
  pixelscope::core::ImageModel image_model_;
  pixelscope::core::ImageHistogram histogram_;
  pixelscope::core::ImageStatistics statistics_;
  pixelscope::core::ViewState view_;
  bool view_initialized_ = false;
  bool histogram_ready_ = false;
  bool statistics_ready_ = false;
  bool show_histogram_ = false;
  bool show_statistics_ = false;
  bool show_pixel_grid_ = false;
  bool pixel_grid_manually_disabled_ = false;
  bool show_dng_cfa_colors_ = false;
  int open_dialog_delay_frames_ = 0;
  float renderer_scale_ = 1.0f;
  float ui_scale_ = 1.0f;
  ImGuiStyle base_style_ = {};
  pixelscope::render::TextureCache texture_cache_;
  std::string renderer_name_;
  std::string last_error_;
  HoverState hover_;
};

}  // namespace pixelscope::ui
