#include "render/texture_cache.h"

namespace pixelscope::render {

TextureCache::~TextureCache() { clear(); }

SDL_Texture* TextureCache::ensure_texture(SDL_Renderer* renderer, const pixelscope::core::ImageData& image) {
  if (!image.valid()) {
    return nullptr;
  }

  const auto& metadata = image.metadata();
  if (texture_ != nullptr && metadata.source_path == source_path_ && metadata.width == width_ &&
      metadata.height == height_) {
    return texture_;
  }

  clear();
  texture_ = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, metadata.width, metadata.height);
  if (texture_ == nullptr) {
    return nullptr;
  }

  SDL_SetTextureScaleMode(texture_, SDL_ScaleModeNearest);
  SDL_UpdateTexture(texture_, nullptr, image.pixels_rgba8().data(), metadata.width * 4);
  source_path_ = metadata.source_path;
  width_ = metadata.width;
  height_ = metadata.height;
  return texture_;
}

void TextureCache::clear() {
  if (texture_ != nullptr) {
    SDL_DestroyTexture(texture_);
    texture_ = nullptr;
  }
  source_path_.clear();
  width_ = 0;
  height_ = 0;
}

}  // namespace pixelscope::render
