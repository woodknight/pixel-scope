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
#include "io/binary_raw_loader.h"
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

  struct PendingRawImport {
    std::string path;
    char width[32] = {};
    char height[32] = {};
    int bit_width_index = 1;
    int cfa_index = 0;
    int endianness_index = 0;
    bool open_popup = false;
  };

  bool load_image(const std::string& path);
  bool load_image(
      const std::string& path,
      const std::optional<pixelscope::io::BinaryRawParameters>& binary_raw_parameters);
  void queue_image_open(const std::string& path);
  void queue_raw_import_dialog(const std::string& path);
  void draw_raw_import_dialog();
  [[nodiscard]] std::optional<pixelscope::io::BinaryRawParameters> parse_pending_raw_import(
      std::string& error_message) const;
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
  void draw_minimap_overlay(
      const pixelscope::core::Rect& canvas_rect,
      const pixelscope::core::Rect& image_bounds,
      SDL_Texture* texture,
      int image_width,
      int image_height);
  void draw_statistics_overlay(const pixelscope::core::Rect& canvas_rect);
  void draw_metadata_overlay(const pixelscope::core::Rect& canvas_rect);
  void draw_status_bar();
  void refresh_raw_bayer_rendering();
  void rebuild_render_image_model();
  [[nodiscard]] const pixelscope::core::ImageModel& active_render_image_model() const;
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
  bool show_metadata_overlay_ = false;
  bool show_pixel_grid_ = false;
  bool pixel_grid_manually_disabled_ = false;
  bool show_raw_cfa_colors_ = false;
  bool auto_contrast_enabled_ = false;
  int open_dialog_delay_frames_ = 0;
  float renderer_scale_ = 1.0f;
  float ui_scale_ = 1.0f;
  ImGuiStyle base_style_ = {};
  pixelscope::core::ImageModel render_image_model_;
  pixelscope::render::TextureCache texture_cache_;
  std::string renderer_name_;
  std::string last_error_;
  HoverState hover_;
  std::optional<PendingRawImport> pending_raw_import_;
};

}  // namespace pixelscope::ui
