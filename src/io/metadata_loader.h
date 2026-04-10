#pragma once

#include <string>
#include <vector>

#include "core/image.h"

namespace pixelscope::io {

[[nodiscard]] std::vector<pixelscope::core::MetadataEntry> load_embedded_metadata(const std::string& path);
void merge_metadata_entries(
    std::vector<pixelscope::core::MetadataEntry>& destination,
    std::vector<pixelscope::core::MetadataEntry> source);

}  // namespace pixelscope::io
