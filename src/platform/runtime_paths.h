#pragma once

#include <filesystem>
#include <string_view>

namespace pixelscope::platform {

[[nodiscard]] std::filesystem::path config_directory();
[[nodiscard]] std::filesystem::path imgui_ini_path();
[[nodiscard]] std::filesystem::path resolve_companion_binary(
    std::string_view filename,
    std::string_view build_fallback_path);

}  // namespace pixelscope::platform
