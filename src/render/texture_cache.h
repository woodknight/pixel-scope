#pragma once

#include <SDL.h>

#include <string>

#include "core/image.h"

namespace pixelscope::render {

class TextureCache {
 public:
  ~TextureCache();

  TextureCache() = default;
  TextureCache(const TextureCache&) = delete;
  TextureCache& operator=(const TextureCache&) = delete;

  [[nodiscard]] SDL_Texture* ensure_texture(SDL_Renderer* renderer, const pixelscope::core::ImageData& image);
  void clear();

 private:
  std::string source_path_;
  int width_ = 0;
  int height_ = 0;
  SDL_Texture* texture_ = nullptr;
};

}  // namespace pixelscope::render
