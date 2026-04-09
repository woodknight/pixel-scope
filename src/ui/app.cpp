#include "ui/app.h"

#include <SDL.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <utility>

#include <backends/imgui_impl_sdl2.h>
#include <backends/imgui_impl_sdlrenderer2.h>

#include "io/file_dialog.h"
#include "io/image_loader.h"

namespace pixelscope::ui {

namespace {

constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 800;
constexpr float kZoomStep = 1.15f;
constexpr float kBaseFontSize = 16.0f;
constexpr float kBaseStatusBarHeight = 28.0f;

pixelscope::core::Rect to_rect(const ImVec2& min, const ImVec2& size) {
  return {.x = min.x, .y = min.y, .w = size.x, .h = size.y};
}

pixelscope::core::Vec2 to_vec2(const ImVec2& value) { return {.x = value.x, .y = value.y}; }

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

  renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (renderer_ == nullptr) {
    renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_SOFTWARE);
  }
  if (renderer_ == nullptr) {
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
    load_image(*initial_path_);
  }
  return true;
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
          load_image(*path);
        }
      }
    }
  }

  return 0;
}

bool App::load_image(const std::string& path) {
  const auto result = pixelscope::io::load_image_file(path);
  if (!result.ok()) {
    last_error_ = result.error_message;
    return false;
  }

  image_ = result.image;
  texture_cache_.clear();
  last_error_.clear();
  reset_hover();
  view_ = {};
  view_initialized_ = false;
  return true;
}

void App::fit_image_to_canvas(float width, float height) {
  if (!image_.valid()) {
    return;
  }

  view_.zoom = pixelscope::core::fit_zoom(image_.metadata().width, image_.metadata().height, width, height);
  view_.pan = {};
  view_initialized_ = true;
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
      load_image(event.drop.file);
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
  ImGui::End();

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
    const bool has_image = image_.valid();
    if (ImGui::MenuItem("Fit to Window", nullptr, false, has_image)) {
      fit_image_to_canvas(
          ImGui::GetMainViewport()->Size.x,
          ImGui::GetMainViewport()->Size.y - ImGui::GetFrameHeight() - (kBaseStatusBarHeight * ui_scale_));
    }
    if (ImGui::MenuItem("1:1 Zoom", nullptr, false, has_image)) {
      view_.zoom = 1.0f;
      view_.pan = {};
      view_initialized_ = true;
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

  if (!image_.valid()) {
    const char* prompt = "Open a PNG or JPEG to inspect pixels.";
    const ImVec2 text_size = ImGui::CalcTextSize(prompt);
    draw_list->AddText(
        ImVec2(canvas_pos.x + (canvas_size.x - text_size.x) * 0.5f, canvas_pos.y + (canvas_size.y - text_size.y) * 0.5f),
        IM_COL32(210, 214, 220, 255),
        prompt);
    ImGui::EndChild();
    return;
  }

  if (!view_initialized_) {
    fit_image_to_canvas(canvas_size.x, canvas_size.y);
  }

  SDL_Texture* texture = texture_cache_.ensure_texture(renderer_, image_);
  if (texture != nullptr) {
    const auto image_bounds =
        pixelscope::core::image_rect(image_.metadata().width, image_.metadata().height, canvas_rect, view_);
    draw_list->AddImage(
        reinterpret_cast<ImTextureID>(texture),
        ImVec2(image_bounds.x, image_bounds.y),
        ImVec2(image_bounds.x + image_bounds.w, image_bounds.y + image_bounds.h));
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
          image_.metadata().width,
          image_.metadata().height,
          canvas_rect);
      view_initialized_ = true;
    }

    const auto image_point = pixelscope::core::screen_to_image(
        to_vec2(ImGui::GetMousePos()),
        image_.metadata().width,
        image_.metadata().height,
        canvas_rect,
        view_);
    if (image_point) {
      const int pixel_x = static_cast<int>(std::floor(image_point->x));
      const int pixel_y = static_cast<int>(std::floor(image_point->y));
      if (const auto pixel = image_.pixel_at(pixel_x, pixel_y)) {
        hover_ = {.x = pixel_x, .y = pixel_y, .pixel = *pixel, .active = true};
      } else {
        reset_hover();
      }
    } else {
      reset_hover();
    }
  } else {
    reset_hover();
  }

  ImGui::EndChild();
}

void App::draw_status_bar() {
  if (image_.valid()) {
    ImGui::Text("Image %dx%d", image_.metadata().width, image_.metadata().height);
    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    ImGui::Text("Zoom %.2fx", view_.zoom);
    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    if (hover_.active) {
      ImGui::Text("Pixel (%d, %d) RGB [%u, %u, %u]",
          hover_.x,
          hover_.y,
          hover_.pixel.r,
          hover_.pixel.g,
          hover_.pixel.b);
    } else {
      ImGui::TextUnformatted("Pixel (-, -)");
    }
  } else if (!last_error_.empty()) {
    ImGui::TextColored(ImVec4(0.92f, 0.38f, 0.38f, 1.0f), "%s", last_error_.c_str());
  } else {
    ImGui::TextUnformatted("No image loaded");
  }
}

void App::reset_hover() { hover_ = {}; }

}  // namespace pixelscope::ui
