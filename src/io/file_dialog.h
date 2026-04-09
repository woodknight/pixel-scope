#pragma once

#include <optional>
#include <string>

namespace pixelscope::io {

[[nodiscard]] std::optional<std::string> open_image_dialog();

}  // namespace pixelscope::io
