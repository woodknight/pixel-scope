#include "platform/runtime_paths.h"

#include <SDL.h>

#include <cstdlib>
#include <string>
#include <system_error>

namespace pixelscope::platform {

namespace {

class ScopedSdlString {
 public:
  explicit ScopedSdlString(char* value) : value_(value) {}
  ScopedSdlString(const ScopedSdlString&) = delete;
  ScopedSdlString& operator=(const ScopedSdlString&) = delete;

  ~ScopedSdlString() {
    if (value_ != nullptr) {
      SDL_free(value_);
    }
  }

  [[nodiscard]] const char* get() const { return value_; }

 private:
  char* value_ = nullptr;
};

std::filesystem::path ensure_directory(const std::filesystem::path& path) {
  if (path.empty()) {
    return {};
  }

  std::error_code error_code;
  std::filesystem::create_directories(path, error_code);
  if (error_code) {
    return {};
  }
  return path;
}

}  // namespace

std::filesystem::path config_directory() {
  std::filesystem::path configured_path;

#if defined(_WIN32)
  const char* appdata = std::getenv("APPDATA");
  if (appdata != nullptr && *appdata != '\0') {
    configured_path = std::filesystem::path(appdata) / "PixelScope";
  }
#elif defined(__APPLE__)
  const char* home = std::getenv("HOME");
  if (home != nullptr && *home != '\0') {
    configured_path = std::filesystem::path(home) / "Library" / "Application Support" / "PixelScope";
  }
#else
  const char* xdg_config_home = std::getenv("XDG_CONFIG_HOME");
  if (xdg_config_home != nullptr && *xdg_config_home != '\0') {
    configured_path = std::filesystem::path(xdg_config_home) / "pixelscope";
  } else {
    const char* home = std::getenv("HOME");
    if (home != nullptr && *home != '\0') {
      configured_path = std::filesystem::path(home) / ".config" / "pixelscope";
    }
  }
#endif

  configured_path = ensure_directory(configured_path);
  if (!configured_path.empty()) {
    return configured_path;
  }

  ScopedSdlString pref_path(SDL_GetPrefPath("PixelScope", "PixelScope"));
  if (pref_path.get() != nullptr) {
    const auto fallback = ensure_directory(pref_path.get());
    if (!fallback.empty()) {
      return fallback;
    }
  }

  return std::filesystem::current_path();
}

std::filesystem::path imgui_ini_path() { return config_directory() / "imgui.ini"; }

std::filesystem::path resolve_companion_binary(
    std::string_view filename,
    std::string_view build_fallback_path) {
  ScopedSdlString base_path(SDL_GetBasePath());
  if (base_path.get() != nullptr) {
    const auto installed_path = std::filesystem::path(base_path.get()) / std::string(filename);
    std::error_code error_code;
    if (std::filesystem::exists(installed_path, error_code) && !error_code) {
      return installed_path;
    }
  }

  if (!build_fallback_path.empty()) {
    return std::filesystem::path(build_fallback_path);
  }
  if (!filename.empty()) {
    return std::filesystem::path(filename);
  }
  return {};
}

}  // namespace pixelscope::platform
