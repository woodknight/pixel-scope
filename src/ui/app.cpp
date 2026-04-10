#include "ui/app.h"

#include <SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <limits>
#include <optional>
#include <string>
#include <utility>

#include <backends/imgui_impl_sdl2.h>
#include <backends/imgui_impl_sdlrenderer2.h>

#include "io/dng_loader.h"
#include "io/file_dialog.h"
#include "io/image_loader.h"

namespace pixelscope::ui {

namespace {

constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 800;
constexpr float kZoomStep = 1.15f;
constexpr float kBaseFontSize = 16.0f;
constexpr float kBaseStatusBarHeight = 28.0f;
constexpr float kGridMinZoom = 8.0f;
constexpr float kHistogramOverlayWidth = 240.0f;
constexpr float kHistogramOverlayHeight = 128.0f;
constexpr float kHistogramOverlayMargin = 16.0f;
constexpr float kMinimapMaxWidth = 180.0f;
constexpr float kMinimapMaxHeight = 180.0f;
constexpr float kMinimapMinWidth = 96.0f;
constexpr float kMinimapMinHeight = 96.0f;
constexpr float kMinimapPadding = 8.0f;
constexpr float kHoverOverlayMargin = 16.0f;
constexpr float kStatisticsOverlayWidth = 260.0f;
constexpr float kMetadataOverlayWidth = 360.0f;
constexpr float kMetadataOverlayMaxHeight = 420.0f;
constexpr int kMaxGridLinesPerAxis = 4096;
constexpr const char* kRawImportPopupName = "Import Binary Bayer Raw";

const std::array<const char*, 4>& cfa_pattern_labels() {
  static const std::array<const char*, 4> labels = {"RGGB", "BGGR", "GRBG", "GBRG"};
  return labels;
}

const std::array<const char*, 2>& raw_bit_width_labels() {
  static const std::array<const char*, 2> labels = {"8-bit", "16-bit"};
  return labels;
}

int cfa_pattern_to_index(const std::array<int, 4>& pattern) {
  if (pattern == std::array<int, 4>{0, 1, 1, 2}) {
    return 0;
  }
  if (pattern == std::array<int, 4>{2, 1, 1, 0}) {
    return 1;
  }
  if (pattern == std::array<int, 4>{1, 0, 2, 1}) {
    return 2;
  }
  if (pattern == std::array<int, 4>{1, 2, 0, 1}) {
    return 3;
  }
  return 0;
}

std::array<int, 4> cfa_index_to_pattern(int index) {
  switch (index) {
    case 1:
      return {2, 1, 1, 0};
    case 2:
      return {1, 0, 2, 1};
    case 3:
      return {1, 2, 0, 1};
    default:
      return {0, 1, 1, 2};
  }
}

void write_int_guess(char* destination, std::size_t destination_size, int value) {
  if (destination == nullptr || destination_size == 0) {
    return;
  }
  destination[0] = '\0';
  if (value > 0) {
    std::snprintf(destination, destination_size, "%d", value);
  }
}

int bit_width_to_index(int bits_per_sample) {
  return bits_per_sample == 8 ? 0 : 1;
}

int bit_width_from_index(int index) {
  return index == 0 ? 8 : 16;
}

pixelscope::core::Rect to_rect(const ImVec2& min, const ImVec2& size) {
  return {.x = min.x, .y = min.y, .w = size.x, .h = size.y};
}

pixelscope::core::Vec2 to_vec2(const ImVec2& value) { return {.x = value.x, .y = value.y}; }

std::uint8_t raw_sample_display_value(std::uint16_t sample, int bits_per_channel) {
  const int bits = std::max(1, bits_per_channel);
  if (bits <= 8) {
    return static_cast<std::uint8_t>(std::min<int>(sample, 255));
  }

  return static_cast<std::uint8_t>(std::min<int>(sample >> (bits - 8), 255));
}

const char* cfa_label(const pixelscope::core::ImageMetadata& metadata, int x, int y) {
  if (!metadata.is_raw_bayer_plane) {
    return "";
  }

  const int pattern_index = (y % 2) * 2 + (x % 2);
  const int channel = metadata.cfa_pattern[static_cast<std::size_t>(pattern_index)];
  if (channel == 0) {
    return "R";
  }
  if (channel == 1) {
    return "G";
  }
  if (channel == 2) {
    return "B";
  }
  return "?";
}

const char* statistics_value_label(const pixelscope::core::ImageStatistics& statistics) {
  if (statistics.uses_raw_samples) {
    return "Raw";
  }
  if (statistics.uses_high_precision_luminance) {
    return "Luma16";
  }
  return "Luma";
}

bool image_has_alpha_channel(const pixelscope::core::ImageMetadata& metadata) {
  return metadata.original_channel_count == 2 || metadata.original_channel_count == 4;
}

void draw_statistics_row(const char* label, const char* value) {
  ImGui::TextUnformatted(label);
  ImGui::SameLine(120.0f * ImGui::GetIO().FontGlobalScale);
  ImGui::TextUnformatted(value);
}

void draw_metadata_row(const char* label, const std::string& value) {
  if (label == nullptr || value.empty()) {
    return;
  }

  const float label_width = 132.0f * ImGui::GetIO().FontGlobalScale;
  const float value_spacing = 18.0f * ImGui::GetIO().FontGlobalScale;
  const float value_x = ImGui::GetCursorPosX() + label_width + value_spacing;

  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted(label);
  ImGui::SameLine(value_x);
  ImGui::PushTextWrapPos(ImGui::GetCursorScreenPos().x +
                         ImGui::GetContentRegionAvail().x);
  ImGui::TextWrapped("%s", value.c_str());
  ImGui::PopTextWrapPos();
}

void draw_pixel_grid_overlay(
    ImDrawList* draw_list,
    const pixelscope::core::Rect& image_bounds,
    const pixelscope::core::Rect& canvas_rect,
    int image_width,
    int image_height,
    float zoom) {
  if (draw_list == nullptr || image_width <= 0 || image_height <= 0 || zoom < kGridMinZoom) {
    return;
  }

  const int start_x = std::max(0, static_cast<int>(std::floor((canvas_rect.x - image_bounds.x) / zoom)));
  const int end_x =
      std::min(image_width, static_cast<int>(std::ceil((canvas_rect.x + canvas_rect.w - image_bounds.x) / zoom)));
  const int start_y = std::max(0, static_cast<int>(std::floor((canvas_rect.y - image_bounds.y) / zoom)));
  const int end_y =
      std::min(image_height, static_cast<int>(std::ceil((canvas_rect.y + canvas_rect.h - image_bounds.y) / zoom)));

  if (end_x < start_x || end_y < start_y) {
    return;
  }
  if ((end_x - start_x) > kMaxGridLinesPerAxis || (end_y - start_y) > kMaxGridLinesPerAxis) {
    return;
  }

  const ImU32 grid_color = IM_COL32(255, 255, 255, 48);
  for (int x = start_x; x <= end_x; ++x) {
    const float screen_x = image_bounds.x + static_cast<float>(x) * zoom;
    draw_list->AddLine(
        ImVec2(screen_x, std::max(image_bounds.y, canvas_rect.y)),
        ImVec2(screen_x, std::min(image_bounds.y + image_bounds.h, canvas_rect.y + canvas_rect.h)),
        grid_color,
        1.0f);
  }

  for (int y = start_y; y <= end_y; ++y) {
    const float screen_y = image_bounds.y + static_cast<float>(y) * zoom;
    draw_list->AddLine(
        ImVec2(std::max(image_bounds.x, canvas_rect.x), screen_y),
        ImVec2(std::min(image_bounds.x + image_bounds.w, canvas_rect.x + canvas_rect.w), screen_y),
        grid_color,
        1.0f);
  }
}

}  // namespace

App::App(std::optional<std::string> initial_path) : initial_path_(std::move(initial_path)) {}

App::~App() {
  ImGui_ImplSDLRenderer2_Shutdown();
  ImGui_ImplSDL2_Shutdown();
  ImGui::DestroyContext();

  if (renderer_ != nullptr) {
    SDL_DestroyRenderer(renderer_);
  }
  if (window_ != nullptr) {
    SDL_DestroyWindow(window_);
  }
  SDL_Quit();
}

bool App::initialize() {
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    last_error_ = SDL_GetError();
    return false;
  }

  window_ = SDL_CreateWindow("PixelScope", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, kWindowWidth, kWindowHeight,
      SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
  if (window_ == nullptr) {
    last_error_ = SDL_GetError();
    return false;
  }

  if (!create_renderer()) {
    last_error_ = SDL_GetError();
    return false;
  }

  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

  ImGui::StyleColorsDark();
  ImGuiStyle& style = ImGui::GetStyle();
  style.WindowRounding = 4.0f;
  style.FrameRounding = 4.0f;
  style.Colors[ImGuiCol_WindowBg] = ImVec4(0.08f, 0.09f, 0.11f, 1.0f);
  base_style_ = style;

  ImGui_ImplSDL2_InitForSDLRenderer(window_, renderer_);
  ImGui_ImplSDLRenderer2_Init(renderer_);
  update_renderer_scale();
  apply_ui_scale(compute_ui_scale());

  if (initial_path_) {
    queue_image_open(*initial_path_);
  }
  return true;
}

bool App::create_renderer() {
  if (window_ == nullptr) {
    return false;
  }

#if defined(__APPLE__)
  const std::vector<std::pair<const char*, std::uint32_t>> attempts = {
      {"metal", SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC},
      {"opengl", SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC},
      {"opengles2", SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC},
      {nullptr, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC},
      {nullptr, SDL_RENDERER_ACCELERATED},
      {"software", SDL_RENDERER_SOFTWARE},
  };
#elif defined(__linux__)
  const std::vector<std::pair<const char*, std::uint32_t>> attempts = {
      {"opengl", SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC},
      {"opengles2", SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC},
      {nullptr, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC},
      {nullptr, SDL_RENDERER_ACCELERATED},
      {"software", SDL_RENDERER_SOFTWARE},
  };
#elif defined(_WIN32)
  const std::vector<std::pair<const char*, std::uint32_t>> attempts = {
      {"direct3d11", SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC},
      {"direct3d", SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC},
      {nullptr, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC},
      {nullptr, SDL_RENDERER_ACCELERATED},
      {"software", SDL_RENDERER_SOFTWARE},
  };
#else
  const std::vector<std::pair<const char*, std::uint32_t>> attempts = {
      {nullptr, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC},
      {nullptr, SDL_RENDERER_ACCELERATED},
      {"software", SDL_RENDERER_SOFTWARE},
  };
#endif

  std::string errors;
  for (const auto& [driver_name, flags] : attempts) {
    if (driver_name != nullptr) {
      SDL_SetHint(SDL_HINT_RENDER_DRIVER, driver_name);
    } else {
      SDL_SetHint(SDL_HINT_RENDER_DRIVER, "");
    }

    renderer_ = SDL_CreateRenderer(window_, -1, flags);
    if (renderer_ == nullptr) {
      if (!errors.empty()) {
        errors += '\n';
      }
      errors += driver_name != nullptr ? driver_name : "auto";
      errors += ": ";
      errors += SDL_GetError();
      continue;
    }

    SDL_RendererInfo info{};
    if (SDL_GetRendererInfo(renderer_, &info) == 0) {
      renderer_name_ = info.name != nullptr ? info.name : "";
    } else if (driver_name != nullptr) {
      renderer_name_ = driver_name;
    } else {
      renderer_name_ = "unknown";
    }

    SDL_Log("PixelScope renderer: %s", renderer_name_.c_str());
    return true;
  }

  last_error_ = errors;
  return false;
}

int App::run() {
  if (!last_error_.empty() && window_ == nullptr) {
    SDL_Log("Initialization failed: %s", last_error_.c_str());
    return 1;
  }

  bool running = true;
  while (running) {
    SDL_Event event;
    while (SDL_PollEvent(&event) != 0) {
      ImGui_ImplSDL2_ProcessEvent(&event);
      bool request_open_dialog = false;
      process_event(event, running, request_open_dialog);
      if (request_open_dialog) {
        open_dialog_delay_frames_ = 2;
      }
    }

    update_renderer_scale();
    update_ui_scale_if_needed();

    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    bool request_open_dialog = false;
    draw_ui(request_open_dialog);
    if (request_open_dialog) {
      open_dialog_delay_frames_ = 2;
    }

    ImGui::Render();
    SDL_SetRenderDrawColor(renderer_, 18, 20, 24, 255);
    SDL_RenderClear(renderer_);
    ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer_);
    SDL_RenderPresent(renderer_);

    if (open_dialog_delay_frames_ > 0) {
      --open_dialog_delay_frames_;
      if (open_dialog_delay_frames_ == 0) {
        if (const auto path = pixelscope::io::open_image_dialog()) {
          queue_image_open(*path);
        }
      }
    }
  }

  return 0;
}

bool App::load_image(const std::string& path) {
  return load_image(path, std::nullopt);
}

bool App::load_image(
    const std::string& path,
    const std::optional<pixelscope::io::BinaryRawParameters>& binary_raw_parameters) {
  auto result = pixelscope::io::load_image_file(path, binary_raw_parameters);
  if (!result.ok()) {
    last_error_ = result.error_message;
    return false;
  }

  if (result.image.metadata().is_raw_bayer_plane && !show_raw_cfa_colors_) {
    result.image = pixelscope::io::render_raw_bayer_image(result.image, false);
  }

  image_model_ = pixelscope::core::build_image_model(std::move(result.image));
  rebuild_render_image_model();
  histogram_ = {};
  histogram_ready_ = false;
  statistics_ = {};
  statistics_ready_ = false;
  texture_cache_.clear();
  last_error_.clear();
  reset_hover();
  view_ = {};
  view_initialized_ = false;
  return true;
}

void App::queue_image_open(const std::string& path) {
  if (pixelscope::io::is_binary_raw_file_path(path)) {
    queue_raw_import_dialog(path);
    return;
  }

  pending_raw_import_.reset();
  load_image(path);
}

void App::queue_raw_import_dialog(const std::string& path) {
  const auto guess = pixelscope::io::guess_binary_raw_parameters_from_filename(path);

  PendingRawImport pending;
  pending.path = path;
  write_int_guess(pending.width, sizeof(pending.width), guess.parameters.width);
  write_int_guess(pending.height, sizeof(pending.height), guess.parameters.height);
  pending.bit_width_index = bit_width_to_index(guess.parameters.bits_per_sample);
  pending.cfa_index = cfa_pattern_to_index(guess.parameters.cfa_pattern);
  pending.endianness_index = guess.parameters.little_endian ? 0 : 1;
  pending.open_popup = true;
  pending_raw_import_ = std::move(pending);
}

std::optional<pixelscope::io::BinaryRawParameters> App::parse_pending_raw_import(std::string& error_message) const {
  if (!pending_raw_import_.has_value()) {
    error_message = "No binary raw import is pending.";
    return std::nullopt;
  }

  const auto parse_positive_int = [&](const char* label, const char* value) -> std::optional<int> {
    if (value == nullptr || value[0] == '\0') {
      error_message = std::string(label) + " is required.";
      return std::nullopt;
    }

    const std::string raw_value(value);
    std::size_t parsed_length = 0;
    int parsed = 0;
    try {
      parsed = std::stoi(raw_value, &parsed_length);
    } catch (...) {
      error_message = std::string(label) + " must be a positive integer.";
      return std::nullopt;
    }
    if (parsed_length != raw_value.size() || parsed <= 0) {
      error_message = std::string(label) + " must be a positive integer.";
      return std::nullopt;
    }
    return parsed;
  };

  auto width = parse_positive_int("Width", pending_raw_import_->width);
  if (!width.has_value()) {
    return std::nullopt;
  }
  auto height = parse_positive_int("Height", pending_raw_import_->height);
  if (!height.has_value()) {
    return std::nullopt;
  }
  return pixelscope::io::BinaryRawParameters{
      .width = *width,
      .height = *height,
      .bits_per_sample = bit_width_from_index(pending_raw_import_->bit_width_index),
      .little_endian = pending_raw_import_->endianness_index == 0,
      .cfa_pattern = cfa_index_to_pattern(pending_raw_import_->cfa_index),
  };
}

void App::draw_raw_import_dialog() {
  if (!pending_raw_import_.has_value()) {
    return;
  }

  if (pending_raw_import_->open_popup) {
    ImGui::OpenPopup(kRawImportPopupName);
    pending_raw_import_->open_popup = false;
  }

  ImGui::SetNextWindowSize(ImVec2(460.0f * ui_scale_, 0.0f), ImGuiCond_Appearing);
  if (!ImGui::BeginPopupModal(kRawImportPopupName, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    return;
  }

  ImGui::TextWrapped("Import settings for %s", pending_raw_import_->path.c_str());
  ImGui::Separator();
  ImGui::InputText("Width", pending_raw_import_->width, sizeof(pending_raw_import_->width));
  ImGui::InputText("Height", pending_raw_import_->height, sizeof(pending_raw_import_->height));
  ImGui::Combo(
      "Bit Width",
      &pending_raw_import_->bit_width_index,
      raw_bit_width_labels().data(),
      static_cast<int>(raw_bit_width_labels().size()));
  ImGui::Combo("CFA Pattern", &pending_raw_import_->cfa_index, cfa_pattern_labels().data(), static_cast<int>(cfa_pattern_labels().size()));
  const char* endianness_labels[] = {"Little-endian", "Big-endian"};
  ImGui::Combo("Byte Order", &pending_raw_import_->endianness_index, endianness_labels, 2);

  ImGui::Spacing();
  if (ImGui::Button("Open", ImVec2(120.0f * ui_scale_, 0.0f))) {
    std::string parse_error;
    const auto parameters = parse_pending_raw_import(parse_error);
    if (!parameters.has_value()) {
      last_error_ = std::move(parse_error);
    } else if (load_image(pending_raw_import_->path, parameters)) {
      pending_raw_import_.reset();
      ImGui::CloseCurrentPopup();
    }
  }
  ImGui::SameLine();
  if (ImGui::Button("Cancel", ImVec2(120.0f * ui_scale_, 0.0f))) {
    pending_raw_import_.reset();
    ImGui::CloseCurrentPopup();
  }

  ImGui::EndPopup();
}

void App::refresh_raw_bayer_rendering() {
  if (!image_model_.valid()) {
    return;
  }

  const auto& source = image_model_.source;
  if (!source.metadata().is_raw_bayer_plane) {
    return;
  }

  image_model_ = pixelscope::core::build_image_model(
      pixelscope::io::render_raw_bayer_image(source, show_raw_cfa_colors_));
  rebuild_render_image_model();
  histogram_ = {};
  histogram_ready_ = false;
  statistics_ = {};
  statistics_ready_ = false;
  texture_cache_.clear();
  reset_hover();
}

void App::maybe_enable_pixel_grid_for_zoom() {
  if (!image_model_.valid() || show_pixel_grid_ || pixel_grid_manually_disabled_) {
    return;
  }

  if (view_.zoom >= kGridMinZoom) {
    show_pixel_grid_ = true;
  }
}

void App::fit_image_to_canvas(float width, float height) {
  if (!image_model_.valid()) {
    return;
  }

  view_.zoom = pixelscope::core::fit_zoom(
      image_model_.source.metadata().width,
      image_model_.source.metadata().height,
      width,
      height);
  view_.pan = {};
  view_initialized_ = true;
  maybe_enable_pixel_grid_for_zoom();
}

float App::compute_renderer_scale() const {
  if (window_ == nullptr || renderer_ == nullptr) {
    return 1.0f;
  }

  int window_width = 0;
  int window_height = 0;
  int output_width = 0;
  int output_height = 0;
  SDL_GetWindowSize(window_, &window_width, &window_height);
  if (window_width <= 0 || window_height <= 0) {
    return 1.0f;
  }
  if (SDL_GetRendererOutputSize(renderer_, &output_width, &output_height) != 0 ||
      output_width <= 0 || output_height <= 0) {
    return 1.0f;
  }

  const float scale_x = static_cast<float>(output_width) / static_cast<float>(window_width);
  const float scale_y = static_cast<float>(output_height) / static_cast<float>(window_height);
  return std::max(1.0f, std::max(scale_x, scale_y));
}

float App::compute_ui_scale() const {
  if (window_ == nullptr || renderer_ == nullptr) {
    return 1.0f;
  }

  const float renderer_scale = compute_renderer_scale();
  float dpi_scale = 1.0f;
  const int display_index = SDL_GetWindowDisplayIndex(window_);
  if (display_index >= 0) {
    float diagonal_dpi = 0.0f;
    if (SDL_GetDisplayDPI(display_index, &diagonal_dpi, nullptr, nullptr) == 0 && diagonal_dpi > 0.0f) {
      dpi_scale = diagonal_dpi / 96.0f;
    }
  }

  const float adjusted_scale = dpi_scale / std::max(1.0f, renderer_scale);
  return std::clamp(adjusted_scale, 1.0f, 2.0f);
}

std::vector<std::string> App::preferred_font_paths() const {
  return {
      "/usr/share/fonts/truetype/ubuntu/UbuntuSans[wdth,wght].ttf",
      "/usr/share/fonts/truetype/ubuntu/Ubuntu-R.ttf",
      "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
      "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
      "/usr/share/fonts/TTF/DejaVuSans.ttf",
      "/Library/Fonts/Helvetica.ttc",
      "/System/Library/Fonts/SFNS.ttf",
      "C:/Windows/Fonts/segoeui.ttf",
      "C:/Windows/Fonts/arial.ttf",
  };
}

void App::apply_ui_scale(float scale) {
  ImGuiIO& io = ImGui::GetIO();
  io.Fonts->Clear();

  ImFontConfig font_config;
  font_config.SizePixels = kBaseFontSize * scale;
  font_config.OversampleH = 2;
  font_config.OversampleV = 2;
  font_config.PixelSnapH = false;

  bool loaded_font = false;
  for (const auto& path : preferred_font_paths()) {
    if (!std::filesystem::exists(path)) {
      continue;
    }
    if (io.Fonts->AddFontFromFileTTF(path.c_str(), font_config.SizePixels, &font_config) != nullptr) {
      loaded_font = true;
      break;
    }
  }
  if (!loaded_font) {
    io.Fonts->AddFontDefault(&font_config);
  }

  ImGuiStyle scaled_style = base_style_;
  scaled_style.ScaleAllSizes(scale);
  ImGui::GetStyle() = scaled_style;

  ImGui_ImplSDLRenderer2_DestroyFontsTexture();
  ImGui_ImplSDLRenderer2_CreateFontsTexture();
  ui_scale_ = scale;
}

void App::update_renderer_scale() {
  const float next_scale = compute_renderer_scale();
  if (std::fabs(next_scale - renderer_scale_) > 0.01f) {
    SDL_RenderSetScale(renderer_, next_scale, next_scale);
    renderer_scale_ = next_scale;
  }
}

void App::update_ui_scale_if_needed() {
  const float next_scale = compute_ui_scale();
  if (std::fabs(next_scale - ui_scale_) > 0.05f) {
    apply_ui_scale(next_scale);
  }
}

void App::process_event(const SDL_Event& event, bool& running, bool& request_open_dialog) {
  if (event.type == SDL_QUIT) {
    running = false;
    return;
  }

  if (event.type == SDL_DROPFILE) {
    if (event.drop.file != nullptr) {
      queue_image_open(event.drop.file);
      SDL_free(event.drop.file);
    }
    return;
  }

  if (event.type == SDL_KEYDOWN) {
    const bool command = (event.key.keysym.mod & KMOD_CTRL) != 0 || (event.key.keysym.mod & KMOD_GUI) != 0;
    if (command && event.key.keysym.sym == SDLK_o) {
      request_open_dialog = true;
    }
  }
}

void App::draw_ui(bool& request_open_dialog) {
  draw_menu(request_open_dialog);

  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  const float status_bar_height = kBaseStatusBarHeight * ui_scale_;
  const float menu_height = ImGui::GetFrameHeight();
  const ImVec2 viewport_pos = viewport->Pos;
  const ImVec2 viewport_size = viewport->Size;
  const float canvas_height = std::max(0.0f, viewport_size.y - menu_height - status_bar_height);

  ImGui::SetNextWindowPos(ImVec2(viewport_pos.x, viewport_pos.y + menu_height));
  ImGui::SetNextWindowSize(ImVec2(viewport_size.x, canvas_height));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
  ImGuiWindowFlags canvas_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                  ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus |
                                  ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
  ImGui::Begin("CanvasHost", nullptr, canvas_flags);
  ImGui::PopStyleVar();
  draw_canvas();
  draw_raw_import_dialog();
  ImGui::End();

  draw_hover_overlay(ImVec2(viewport_pos.x, viewport_pos.y + menu_height));

  ImGui::SetNextWindowPos(ImVec2(viewport_pos.x, viewport_pos.y + menu_height + canvas_height));
  ImGui::SetNextWindowSize(ImVec2(viewport_size.x, status_bar_height));
  ImGuiWindowFlags status_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                  ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus |
                                  ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
  ImGui::Begin("StatusBar", nullptr, status_flags);
  draw_status_bar();
  ImGui::End();
}

void App::draw_menu(bool& request_open_dialog) {
  if (!ImGui::BeginMainMenuBar()) {
    return;
  }

  if (ImGui::BeginMenu("File")) {
    if (ImGui::MenuItem("Open...", "Ctrl+O")) {
      request_open_dialog = true;
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Quit")) {
      SDL_Event quit_event{};
      quit_event.type = SDL_QUIT;
      SDL_PushEvent(&quit_event);
    }
    ImGui::EndMenu();
  }

  if (ImGui::BeginMenu("View")) {
    const bool has_image = image_model_.valid();
    const bool has_raw_bayer_image = has_image && image_model_.source.metadata().is_raw_bayer_plane;
    if (ImGui::MenuItem("Fit to Window", nullptr, false, has_image)) {
      fit_image_to_canvas(
          ImGui::GetMainViewport()->Size.x,
          ImGui::GetMainViewport()->Size.y - ImGui::GetFrameHeight() - (kBaseStatusBarHeight * ui_scale_));
    }
    if (ImGui::MenuItem("1:1 Zoom", nullptr, false, has_image)) {
      view_.zoom = 1.0f;
      view_.pan = {};
      view_initialized_ = true;
      maybe_enable_pixel_grid_for_zoom();
    }
    ImGui::Separator();
    const bool histogram_enabled_before = show_histogram_;
    const bool statistics_enabled_before = show_statistics_;
    ImGui::MenuItem("Histogram Overlay", nullptr, &show_histogram_, has_image);
    if (show_histogram_ && !histogram_enabled_before && image_model_.valid() && !histogram_ready_) {
      histogram_ = pixelscope::core::compute_histogram(image_model_.source);
      histogram_ready_ = true;
    }
    ImGui::MenuItem("Image Statistics", nullptr, &show_statistics_, has_image);
    if (show_statistics_ && !statistics_enabled_before && image_model_.valid() && !statistics_ready_) {
      statistics_ = pixelscope::core::compute_image_statistics(image_model_.source);
      statistics_ready_ = true;
    }
    ImGui::MenuItem("Metadata Overlay", nullptr, &show_metadata_overlay_, has_image);
    const bool pixel_grid_enabled_before = show_pixel_grid_;
    if (ImGui::MenuItem("Pixel Grid", nullptr, &show_pixel_grid_, has_image)) {
      if (show_pixel_grid_) {
        pixel_grid_manually_disabled_ = false;
      } else if (pixel_grid_enabled_before || view_.zoom >= kGridMinZoom) {
        pixel_grid_manually_disabled_ = true;
      }
    }
    if (ImGui::MenuItem("Auto Contrast", nullptr, &auto_contrast_enabled_, has_image)) {
      rebuild_render_image_model();
      texture_cache_.clear();
    }
    if (ImGui::MenuItem("RAW CFA Colors", nullptr, &show_raw_cfa_colors_, has_raw_bayer_image)) {
      refresh_raw_bayer_rendering();
    }
    ImGui::EndMenu();
  }

  ImGui::EndMainMenuBar();
}

void App::draw_canvas() {
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
  ImGui::BeginChild("Canvas", ImVec2(0.0f, 0.0f), false, ImGuiWindowFlags_NoMove);
  ImGui::PopStyleVar();

  const ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
  const ImVec2 canvas_size = ImGui::GetContentRegionAvail();
  const pixelscope::core::Rect canvas_rect = to_rect(canvas_pos, canvas_size);
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  draw_list->AddRectFilled(canvas_pos, ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y), IM_COL32(24, 26, 32, 255));

  if (!image_model_.valid()) {
    const char* prompt = "Open a PNG, JPEG, TIFF, DNG, or binary Bayer raw to inspect pixels.";
    const ImVec2 text_size = ImGui::CalcTextSize(prompt);
    draw_list->AddText(
        ImVec2(canvas_pos.x + (canvas_size.x - text_size.x) * 0.5f, canvas_pos.y + (canvas_size.y - text_size.y) * 0.5f),
        IM_COL32(210, 214, 220, 255),
        prompt);
    ImGui::EndChild();
    return;
  }

  const auto& source_image = image_model_.source;
  if (!view_initialized_) {
    fit_image_to_canvas(canvas_size.x, canvas_size.y);
  }

  const auto& render_model = active_render_image_model();
  const pixelscope::core::ImageData* display_image = &render_model.source;
  if (const auto* level = render_model.pick_display_level(view_.zoom)) {
    display_image = &level->image;
  }

  SDL_Texture* texture = texture_cache_.ensure_texture(renderer_, *display_image);
  if (texture != nullptr) {
    const auto image_bounds =
        pixelscope::core::image_rect(source_image.metadata().width, source_image.metadata().height, canvas_rect, view_);
    draw_list->AddImage(
        reinterpret_cast<ImTextureID>(texture),
        ImVec2(image_bounds.x, image_bounds.y),
        ImVec2(image_bounds.x + image_bounds.w, image_bounds.y + image_bounds.h));
    if (show_pixel_grid_) {
      draw_pixel_grid_overlay(
          draw_list,
          image_bounds,
          canvas_rect,
          source_image.metadata().width,
          source_image.metadata().height,
          view_.zoom);
    }
    draw_minimap_overlay(
        canvas_rect,
        image_bounds,
        texture,
        source_image.metadata().width,
        source_image.metadata().height);
  }

  const bool hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
  if (hovered) {
    if (ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f)) {
      const ImVec2 delta = ImGui::GetIO().MouseDelta;
      view_.pan.x += delta.x;
      view_.pan.y += delta.y;
      view_initialized_ = true;
    }

    const float wheel = ImGui::GetIO().MouseWheel;
    if (wheel != 0.0f) {
      const float factor = wheel > 0.0f ? kZoomStep : (1.0f / kZoomStep);
      pixelscope::core::zoom_around_point(
          view_,
          view_.zoom * factor,
          to_vec2(ImGui::GetMousePos()),
          source_image.metadata().width,
          source_image.metadata().height,
          canvas_rect);
      view_initialized_ = true;
      maybe_enable_pixel_grid_for_zoom();
    }

    const auto image_point = pixelscope::core::screen_to_image(
        to_vec2(ImGui::GetMousePos()),
        source_image.metadata().width,
        source_image.metadata().height,
        canvas_rect,
        view_);
    if (image_point) {
      const int pixel_x = static_cast<int>(std::floor(image_point->x));
      const int pixel_y = static_cast<int>(std::floor(image_point->y));
      if (const auto pixel = source_image.pixel_at(pixel_x, pixel_y)) {
        hover_ = {
            .x = pixel_x,
            .y = pixel_y,
            .pixel = *pixel,
            .pixel16 = source_image.pixel16_at(pixel_x, pixel_y),
            .raw_sample = source_image.raw_sample_at(pixel_x, pixel_y),
            .active = true,
        };
      } else {
        reset_hover();
      }
    } else {
      reset_hover();
    }
  } else {
    reset_hover();
  }

  draw_histogram_overlay(canvas_rect);
  draw_statistics_overlay(canvas_rect);
  draw_metadata_overlay(canvas_rect);

  ImGui::EndChild();
}

void App::draw_hover_overlay(const ImVec2& canvas_pos) {
  if (!image_model_.valid()) {
    return;
  }

  const auto& source_image = image_model_.source;
  const float margin = kHoverOverlayMargin * ui_scale_;
  ImGui::SetNextWindowPos(ImVec2(canvas_pos.x + margin, canvas_pos.y + margin), ImGuiCond_Always);
  ImGui::SetNextWindowBgAlpha(0.86f);

  ImGuiWindowFlags overlay_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                                   ImGuiWindowFlags_NoNav | ImGuiWindowFlags_AlwaysAutoResize;
  ImGui::Begin("HoverOverlay", nullptr, overlay_flags);

  ImGui::TextUnformatted("Pixel");
  ImGui::Separator();
  if (hover_.active) {
    ImGui::Text("Coord: (%d, %d)", hover_.x, hover_.y);
    if (source_image.metadata().is_raw_bayer_plane && hover_.raw_sample.has_value()) {
      ImGui::Text(
          "Value: %s RAW [%u] Gray [%u]",
          cfa_label(source_image.metadata(), hover_.x, hover_.y),
          hover_.raw_sample.value(),
          raw_sample_display_value(hover_.raw_sample.value(), source_image.metadata().bits_per_channel));
    } else if (hover_.pixel16.has_value()) {
      if (image_has_alpha_channel(source_image.metadata())) {
        ImGui::Text(
            "Value: RGBA16 [%u, %u, %u, %u]",
            hover_.pixel16->r,
            hover_.pixel16->g,
            hover_.pixel16->b,
            hover_.pixel16->a);
      } else {
        ImGui::Text(
            "Value: RGB16 [%u, %u, %u]", hover_.pixel16->r, hover_.pixel16->g, hover_.pixel16->b);
      }
    } else {
      if (image_has_alpha_channel(source_image.metadata())) {
        ImGui::Text("Value: RGBA [%u, %u, %u, %u]", hover_.pixel.r, hover_.pixel.g, hover_.pixel.b, hover_.pixel.a);
      } else {
        ImGui::Text("Value: RGB [%u, %u, %u]", hover_.pixel.r, hover_.pixel.g, hover_.pixel.b);
      }
    }
  } else {
    ImGui::TextUnformatted("Coord: (-, -)");
    ImGui::TextUnformatted("Value: -");
  }

  ImGui::End();
}

void App::draw_minimap_overlay(
    const pixelscope::core::Rect& canvas_rect,
    const pixelscope::core::Rect& image_bounds,
    SDL_Texture* texture,
    int image_width,
    int image_height) {
  if (texture == nullptr || image_width <= 0 || image_height <= 0) {
    return;
  }

  const float visible_min_x = std::max(canvas_rect.x, image_bounds.x);
  const float visible_min_y = std::max(canvas_rect.y, image_bounds.y);
  const float visible_max_x = std::min(canvas_rect.x + canvas_rect.w, image_bounds.x + image_bounds.w);
  const float visible_max_y = std::min(canvas_rect.y + canvas_rect.h, image_bounds.y + image_bounds.h);
  if (visible_min_x >= visible_max_x || visible_min_y >= visible_max_y || image_bounds.w <= 0.0f ||
      image_bounds.h <= 0.0f) {
    return;
  }

  const float visible_u0 = std::clamp((visible_min_x - image_bounds.x) / image_bounds.w, 0.0f, 1.0f);
  const float visible_v0 = std::clamp((visible_min_y - image_bounds.y) / image_bounds.h, 0.0f, 1.0f);
  const float visible_u1 = std::clamp((visible_max_x - image_bounds.x) / image_bounds.w, 0.0f, 1.0f);
  const float visible_v1 = std::clamp((visible_max_y - image_bounds.y) / image_bounds.h, 0.0f, 1.0f);
  if ((visible_u1 - visible_u0) >= 0.999f && (visible_v1 - visible_v0) >= 0.999f) {
    return;
  }

  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  if (draw_list == nullptr) {
    return;
  }

  const float margin = kHistogramOverlayMargin * ui_scale_;
  const float padding = kMinimapPadding * ui_scale_;
  const float available_width = std::max(1.0f, canvas_rect.w - margin * 2.0f - padding * 2.0f);
  const float available_height = std::max(1.0f, canvas_rect.h - margin * 2.0f - padding * 2.0f);
  const float max_thumb_width = std::min(kMinimapMaxWidth * ui_scale_, available_width);
  const float max_thumb_height = std::min(kMinimapMaxHeight * ui_scale_, available_height);
  const float image_aspect = static_cast<float>(image_width) / static_cast<float>(image_height);

  float thumb_width = max_thumb_width;
  float thumb_height = thumb_width / image_aspect;
  if (thumb_height > max_thumb_height) {
    thumb_height = max_thumb_height;
    thumb_width = thumb_height * image_aspect;
  }
  if (thumb_width < std::min(kMinimapMinWidth * ui_scale_, available_width)) {
    thumb_width = std::min(kMinimapMinWidth * ui_scale_, available_width);
    thumb_height = thumb_width / image_aspect;
  }
  if (thumb_height < std::min(kMinimapMinHeight * ui_scale_, available_height)) {
    thumb_height = std::min(kMinimapMinHeight * ui_scale_, available_height);
    thumb_width = thumb_height * image_aspect;
  }
  if (thumb_width > available_width) {
    thumb_width = available_width;
    thumb_height = thumb_width / image_aspect;
  }
  if (thumb_height > available_height) {
    thumb_height = available_height;
    thumb_width = thumb_height * image_aspect;
  }

  const ImVec2 outer_min(
      canvas_rect.x + canvas_rect.w - thumb_width - (padding * 2.0f) - margin,
      canvas_rect.y + canvas_rect.h - thumb_height - (padding * 2.0f) - margin);
  const ImVec2 outer_max(
      outer_min.x + thumb_width + (padding * 2.0f),
      outer_min.y + thumb_height + (padding * 2.0f));
  const ImVec2 thumb_min(outer_min.x + padding, outer_min.y + padding);
  const ImVec2 thumb_max(thumb_min.x + thumb_width, thumb_min.y + thumb_height);

  const ImVec2 fov_min(
      thumb_min.x + visible_u0 * thumb_width,
      thumb_min.y + visible_v0 * thumb_height);
  const ImVec2 fov_max(
      thumb_min.x + visible_u1 * thumb_width,
      thumb_min.y + visible_v1 * thumb_height);

  draw_list->AddRectFilled(outer_min, outer_max, IM_COL32(10, 12, 16, 220), 6.0f);
  draw_list->AddRect(outer_min, outer_max, IM_COL32(190, 194, 204, 90), 6.0f, 0, 1.0f);
  draw_list->AddImage(reinterpret_cast<ImTextureID>(texture), thumb_min, thumb_max);
  draw_list->AddRect(thumb_min, thumb_max, IM_COL32(255, 255, 255, 36), 0.0f, 0, 1.0f);
  draw_list->AddRectFilled(fov_min, fov_max, IM_COL32(255, 255, 255, 26), 0.0f);
  draw_list->AddRect(fov_min, fov_max, IM_COL32(255, 200, 96, 255), 0.0f, 0, 2.0f * ui_scale_);
}

void App::draw_histogram_overlay(const pixelscope::core::Rect& canvas_rect) {
  if (!show_histogram_) {
    return;
  }

  if (!histogram_ready_ && image_model_.valid()) {
    histogram_ = pixelscope::core::compute_histogram(image_model_.source);
    histogram_ready_ = true;
  }

  if (histogram_.empty()) {
    return;
  }

  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  if (draw_list == nullptr) {
    return;
  }

  const float overlay_width = kHistogramOverlayWidth * ui_scale_;
  const float overlay_height = kHistogramOverlayHeight * ui_scale_;
  const float margin = kHistogramOverlayMargin * ui_scale_;
  const float title_height = 20.0f * ui_scale_;
  const float padding = 10.0f * ui_scale_;

  const ImVec2 overlay_min(canvas_rect.x + margin, canvas_rect.y + canvas_rect.h - overlay_height - margin);
  const ImVec2 overlay_max(overlay_min.x + overlay_width, overlay_min.y + overlay_height);
  const ImVec2 plot_min(overlay_min.x + padding, overlay_min.y + title_height + padding * 0.5f);
  const ImVec2 plot_max(overlay_max.x - padding, overlay_max.y - padding);

  draw_list->AddRectFilled(overlay_min, overlay_max, IM_COL32(10, 12, 16, 220), 6.0f);
  draw_list->AddRect(overlay_min, overlay_max, IM_COL32(190, 194, 204, 90), 6.0f, 0, 1.0f);
  draw_list->AddText(
      ImVec2(overlay_min.x + padding, overlay_min.y + 4.0f * ui_scale_),
      IM_COL32(224, 228, 236, 255),
      "Histogram");
  draw_list->AddRect(plot_min, plot_max, IM_COL32(255, 255, 255, 28), 0.0f, 0, 1.0f);

  const auto draw_channel = [&](const pixelscope::core::HistogramChannel& channel, ImU32 color) {
    if (channel.max_count == 0) {
      return;
    }

    for (int index = 1; index < 256; ++index) {
      const float x0 = plot_min.x + (plot_max.x - plot_min.x) * (static_cast<float>(index - 1) / 255.0f);
      const float x1 = plot_min.x + (plot_max.x - plot_min.x) * (static_cast<float>(index) / 255.0f);
      const float y0 = plot_max.y -
                       (plot_max.y - plot_min.y) *
                           (static_cast<float>(channel.bins[static_cast<std::size_t>(index - 1)]) /
                               static_cast<float>(channel.max_count));
      const float y1 = plot_max.y -
                       (plot_max.y - plot_min.y) *
                           (static_cast<float>(channel.bins[static_cast<std::size_t>(index)]) /
                               static_cast<float>(channel.max_count));
      draw_list->AddLine(ImVec2(x0, y0), ImVec2(x1, y1), color, 1.5f * ui_scale_);
    }
  };

  draw_channel(histogram_.luminance, IM_COL32(220, 220, 220, 190));
  draw_channel(histogram_.red, IM_COL32(255, 96, 96, 210));
  draw_channel(histogram_.green, IM_COL32(96, 220, 120, 210));
  draw_channel(histogram_.blue, IM_COL32(96, 156, 255, 210));
}

void App::draw_statistics_overlay(const pixelscope::core::Rect& canvas_rect) {
  if (!show_statistics_) {
    return;
  }

  if (!statistics_ready_ && image_model_.valid()) {
    statistics_ = pixelscope::core::compute_image_statistics(image_model_.source);
    statistics_ready_ = true;
  }

  if (statistics_.empty()) {
    return;
  }

  const float margin = kHistogramOverlayMargin * ui_scale_;
  const float width = kStatisticsOverlayWidth * ui_scale_;
  const ImVec2 overlay_pos(canvas_rect.x + canvas_rect.w - width - margin, canvas_rect.y + margin);

  ImGui::SetNextWindowPos(overlay_pos, ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(width, 0.0f), ImGuiCond_Always);
  ImGui::SetNextWindowBgAlpha(0.86f);

  ImGuiWindowFlags overlay_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                                   ImGuiWindowFlags_NoNav;
  ImGui::Begin("StatisticsOverlay", nullptr, overlay_flags);

  char mean_value[32] = {};
  std::snprintf(mean_value, sizeof(mean_value), "%.2f", statistics_.mean);

  ImGui::TextUnformatted("Statistics");
  ImGui::Separator();
  draw_statistics_row("Mode", statistics_value_label(statistics_));
  ImGui::Text("Samples: %zu", statistics_.sample_count);
  ImGui::Text("Min: %u", statistics_.min_value);
  ImGui::Text("P10: %u", statistics_.percentile_10);
  ImGui::Text("Median: %u", statistics_.median);
  ImGui::Text("Mean: %s", mean_value);
  ImGui::Text("P90: %u", statistics_.percentile_90);
  ImGui::Text("Max: %u", statistics_.max_value);

  ImGui::End();
}

void App::draw_metadata_overlay(const pixelscope::core::Rect& canvas_rect) {
  if (!show_metadata_overlay_ || !image_model_.valid()) {
    return;
  }

  const auto& metadata = image_model_.source.metadata();
  const bool has_extra_metadata = !metadata.metadata_entries.empty();
  if (!has_extra_metadata && metadata.source_path.empty()) {
    return;
  }

  const float margin = kHistogramOverlayMargin * ui_scale_;
  const float width = kMetadataOverlayWidth * ui_scale_;
  const float max_height = std::min(
      kMetadataOverlayMaxHeight * ui_scale_,
      std::max(180.0f * ui_scale_, canvas_rect.h - (margin * 2.0f)));
  const float right_offset = show_statistics_ ? (kStatisticsOverlayWidth * ui_scale_) + margin : 0.0f;
  const ImVec2 overlay_pos(
      canvas_rect.x + canvas_rect.w - width - margin - right_offset,
      canvas_rect.y + margin);

  ImGui::SetNextWindowPos(overlay_pos, ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(width, max_height), ImGuiCond_Always);
  ImGui::SetNextWindowBgAlpha(0.88f);

  ImGuiWindowFlags overlay_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                                   ImGuiWindowFlags_NoNav;
  ImGui::Begin("MetadataOverlay", nullptr, overlay_flags);

  ImGui::TextUnformatted("Metadata");
  ImGui::Separator();
  draw_metadata_row("Path", metadata.source_path);
  draw_metadata_row("Dimensions", std::to_string(metadata.width) + " x " + std::to_string(metadata.height));
  draw_metadata_row("Bit Depth", std::to_string(metadata.bits_per_channel));
  draw_metadata_row("Channels", std::to_string(metadata.original_channel_count));
  draw_metadata_row("Format", metadata.is_raw_bayer_plane ? "RAW Bayer" : "RGBA");

  if (has_extra_metadata) {
    ImGui::Spacing();
    ImGui::BeginChild("MetadataOverlayScroll", ImVec2(0.0f, 0.0f), false, ImGuiWindowFlags_HorizontalScrollbar);
    for (const auto& entry : metadata.metadata_entries) {
      draw_metadata_row(entry.label.c_str(), entry.value);
    }
    ImGui::EndChild();
  }

  ImGui::End();
}

void App::draw_status_bar() {
  if (image_model_.valid()) {
    const auto& source_image = image_model_.source;
    ImGui::Text("Image %dx%d", source_image.metadata().width, source_image.metadata().height);
    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    ImGui::Text("Zoom %.2fx", view_.zoom);
    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    ImGui::Text("Auto Contrast %s", auto_contrast_enabled_ ? "On" : "Off");
    if (!renderer_name_.empty()) {
      ImGui::SameLine();
      ImGui::TextDisabled("|");
      ImGui::SameLine();
      ImGui::Text("Renderer %s", renderer_name_.c_str());
    }
  } else if (!last_error_.empty()) {
    ImGui::TextColored(ImVec4(0.92f, 0.38f, 0.38f, 1.0f), "%s", last_error_.c_str());
  } else {
    ImGui::TextUnformatted("No image loaded");
  }
}

void App::rebuild_render_image_model() {
  if (!image_model_.valid()) {
    render_image_model_ = {};
    return;
  }

  if (auto_contrast_enabled_) {
    render_image_model_ = pixelscope::core::build_auto_contrast_image_model(image_model_);
  } else {
    render_image_model_ = {};
  }
}

const pixelscope::core::ImageModel& App::active_render_image_model() const {
  if (auto_contrast_enabled_ && render_image_model_.valid()) {
    return render_image_model_;
  }
  return image_model_;
}

void App::reset_hover() { hover_ = {}; }

}  // namespace pixelscope::ui
