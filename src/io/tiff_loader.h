#pragma once

#include <string>

#include "io/image_loader.h"

namespace pixelscope::io {

[[nodiscard]] LoadImageResult load_tiff_file(const std::string& path);

}  // namespace pixelscope::io
