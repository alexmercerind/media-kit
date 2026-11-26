// This file is a part of media_kit
// (https://github.com/media-kit/media-kit).
//
// Copyright © 2021 & onwards, Hitesh Kumar Saini <saini123hitesh@gmail.com>.
// All rights reserved.
// Use of this source code is governed by MIT license that can be found in the
// LICENSE file.

#include "video_output.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <type_traits>
#include <utility>

namespace {

template <typename T, typename = void>
struct has_registrar_get_graphics_adapter : std::false_type {};

template <typename T>
struct has_registrar_get_graphics_adapter<
    T,
    std::void_t<decltype(std::declval<T*>()->GetGraphicsAdapter(
        std::declval<IDXGIAdapter**>()))>> : std::true_type {};

Microsoft::WRL::ComPtr<IDXGIAdapter> GetFlutterGraphicsAdapter(
    flutter::PluginRegistrarWindows* registrar) {
  Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
  if (!registrar) {
    return adapter;
  }
  if constexpr (has_registrar_get_graphics_adapter<
                    flutter::PluginRegistrarWindows>::value) {
    registrar->GetGraphicsAdapter(adapter.GetAddressOf());
  } else if (auto* view = registrar->GetView()) {
    // FlutterDesktopViewGetGraphicsAdapter: caller must release the adapter.
    adapter.Attach(view->GetGraphicsAdapter());
  }
  return adapter;
}

}  // namespace

// Limit the frame size to 1080p in software rendering.
// This is for performance reasons & to avoid allocating too much memory.
#define SW_RENDERING_MAX_WIDTH 1920
#define SW_RENDERING_MAX_HEIGHT 1080
#define SW_RENDERING_PIXEL_BUFFER_SIZE \
  (SW_RENDERING_MAX_WIDTH) * (SW_RENDERING_MAX_HEIGHT) * (4)

VideoOutput::VideoOutput(int64_t handle,
                         VideoOutputConfiguration configuration,
                         flutter::PluginRegistrarWindows* registrar,
                         ThreadPool* thread_pool_ref)
    : handle_(reinterpret_cast<mpv_handle*>(handle)),
      width_(configuration.width),
      height_(configuration.height),
      configuration_(configuration),
      registrar_(registrar),
      thread_pool_ref_(thread_pool_ref) {
  // NOTE: The constructor must be invoked from an external/detached thread,
  // NEVER from ThreadPool's own worker thread, to prevent self-deadlock on .wait().
  auto future = thread_pool_ref_->Post([&]() {
    mpv_set_option_string(handle_, "video-sync", "audio");
    mpv_set_option_string(handle_, "video-timing-offset", "0");

    // Initialize video playback with hardware acceleration using native D3D11.
    auto is_hardware_acceleration_enabled = false;

    if (configuration.enable_hardware_acceleration) {
      try {
        Microsoft::WRL::ComPtr<IDXGIAdapter> flutter_adapter =
            GetFlutterGraphicsAdapter(registrar_);
        if (!flutter_adapter) {
          std::cout << "media_kit: VideoOutput: View returned null graphics adapter. Default adapter will be used."
                    << std::endl;
        }
        d3d11_renderer_ = std::make_unique<D3D11Renderer>(
            static_cast<int32_t>(width_.value_or(1)),
            static_cast<int32_t>(height_.value_or(1)),
            flutter_adapter.Get());

        // Initialize mpv with the D3D11 device. The render target is supplied
        // per frame through MPV_RENDER_PARAM_D3D11_FBO in Render().
        mpv_d3d11_init_params init_params = {d3d11_renderer_->device()};

        // MPV_RENDER_PARAM_RENDERER is left unset on purpose: libmpv then
        // renders with gpu-next (libplacebo's D3D11 backend on this device)
        // and falls back to the gpu renderer when gpu-next cannot initialize.
        mpv_render_param params[] = {
            {MPV_RENDER_PARAM_API_TYPE, MPV_RENDER_API_TYPE_D3D11},
            {MPV_RENDER_PARAM_D3D11_INIT_PARAMS, &init_params},
            {MPV_RENDER_PARAM_INVALID, nullptr},
        };

        // Create render context.
        int create_res =
            mpv_render_context_create(&render_context_, handle_, params);
        if (create_res == 0) {
          mpv_render_context_set_update_callback(
              render_context_,
              [](void* context) {
                auto that = reinterpret_cast<VideoOutput*>(context);
                that->NotifyRender();
              },
              reinterpret_cast<void*>(this));

          // Now create the Flutter texture after successful render context creation
          Resize(width_.value_or(1), height_.value_or(1));

          // Set flag to true, indicating that H/W rendering is supported.
          is_hardware_acceleration_enabled = true;
          std::cout << "media_kit: VideoOutput: Using native D3D11 H/W rendering."
                    << std::endl;
        } else {
          std::cout << "media_kit: VideoOutput: Failed to create mpv render context: "
                    << mpv_error_string(create_res) << " (" << create_res << ")"
                    << std::endl;
          d3d11_renderer_.reset(nullptr);
        }
      } catch (const std::exception& e) {
        // Fallback to software rendering.
        std::cout << "media_kit: VideoOutput: Failed to initialize D3D11: "
                  << e.what() << ", falling back to S/W."
                  << std::endl;
        d3d11_renderer_.reset(nullptr);
      } catch (...) {
        // Fallback to software rendering.
        std::cout << "media_kit: VideoOutput: Failed to initialize D3D11, falling back to S/W."
                  << std::endl;
        d3d11_renderer_.reset(nullptr);
      }
    }

    if (!is_hardware_acceleration_enabled) {
      std::cout << "media_kit: VideoOutput: Using S/W rendering." << std::endl;
      // Allocate a "large enough" buffer ahead of time.
      pixel_buffer_ =
          std::make_unique<uint8_t[]>(SW_RENDERING_PIXEL_BUFFER_SIZE);
      Resize(width_.value_or(1), height_.value_or(1));
      mpv_render_param params[] = {
          {MPV_RENDER_PARAM_API_TYPE, MPV_RENDER_API_TYPE_SW},
          {MPV_RENDER_PARAM_INVALID, nullptr},
      };
      if (mpv_render_context_create(&render_context_, handle_, params) == 0) {
        mpv_render_context_set_update_callback(
            render_context_,
            [](void* context) {
              auto that = reinterpret_cast<VideoOutput*>(context);
              that->NotifyRender();
            },
            reinterpret_cast<void*>(this));
      }
    }
  });
  future.wait();
}

VideoOutput::~VideoOutput() {
  destroyed_ = true;
  {
    std::lock_guard<std::mutex> lock(texture_state_->mutex);
    texture_state_->destroyed = true;
  }
  if (render_context_) {
    // Stop the producer before draining queued tasks that borrow this object.
    mpv_render_context_set_update_callback(render_context_, nullptr, nullptr);
  }
  if (texture_id_) {
    auto promise = std::promise<void>();
    registrar_->texture_registrar()->UnregisterTexture(
        texture_id_, [this, texture_id = texture_id_, &promise]() {
          thread_pool_ref_->Post([this, id = texture_id, &promise]() {
            std::cout << "media_kit: VideoOutput: Free Texture: " << id
                      << std::endl;
            std::lock_guard<std::mutex> lock(texture_state_->mutex);
            texture_state_->texture_variants.clear();
            // H/W
            texture_state_->textures.clear();
            // S/W
            texture_state_->pixel_buffer_textures.clear();
            promise.set_value();
          });
        });
    promise.get_future().wait();
  }
  texture_id_ = 0;

  thread_pool_ref_->Post([this, render_context = render_context_]() {
    if (render_context) {
      mpv_render_context_free(render_context);
    }
    d3d11_renderer_.reset(nullptr);
  }).wait();
}

void VideoOutput::NotifyRender() {
  if (destroyed_) {
    return;
  }
  thread_pool_ref_->Post(std::bind(&VideoOutput::CheckAndResize, this));
  thread_pool_ref_->Post(std::bind(&VideoOutput::Render, this));
}

void VideoOutput::SkipFrame() {
  if (!render_context_) {
    return;
  }
  int skip = 1;
  mpv_render_param params[]{
      {MPV_RENDER_PARAM_SKIP_RENDERING, &skip},
      {MPV_RENDER_PARAM_INVALID, nullptr},
  };
  mpv_render_context_render(render_context_, params);
}

void VideoOutput::Render() {
  if (destroyed_) {
    return;
  }
  if (!texture_id_) {
    SkipFrame();
    return;
  }

  bool rendered = false;
  if (d3d11_renderer_ != nullptr) {
    if (auto rt = d3d11_renderer_->render_target()) {
      mpv_d3d11_fbo fbo = {rt.Get()};
      mpv_render_param params[]{
          {MPV_RENDER_PARAM_D3D11_FBO, &fbo},
          {MPV_RENDER_PARAM_INVALID, nullptr},
      };
      mpv_render_context_render(render_context_, params);
      d3d11_renderer_->Commit();
      rendered = true;
    }
  } else if (pixel_buffer_ != nullptr) {
    int32_t width = 0;
    int32_t height = 0;
    {
      std::lock_guard<std::mutex> lock(texture_state_->mutex);
      auto it = texture_state_->pixel_buffer_textures.find(texture_id_);
      if (it != texture_state_->pixel_buffer_textures.end() &&
          it->second != nullptr) {
        width = static_cast<int32_t>(it->second->width);
        height = static_cast<int32_t>(it->second->height);
      }
    }
    if (width > 0 && height > 0) {
      int32_t size[]{width, height};
      auto pitch = 4 * size[0];
      mpv_render_param params[]{
          {MPV_RENDER_PARAM_SW_SIZE, size},
          {MPV_RENDER_PARAM_SW_FORMAT, "rgb0"},
          {MPV_RENDER_PARAM_SW_STRIDE, &pitch},
          {MPV_RENDER_PARAM_SW_POINTER, pixel_buffer_.get()},
          {MPV_RENDER_PARAM_INVALID, nullptr},
      };
      mpv_render_context_render(render_context_, params);
      rendered = true;
    }
  }

  if (rendered) {
    try {
      // Notify Flutter that a new frame is available.
      registrar_->texture_registrar()->MarkTextureFrameAvailable(texture_id_);
    } catch (...) {
      // Prevent any redundant exceptions if the texture is unregistered etc.
    }
  } else {
    SkipFrame();
  }
}

void VideoOutput::SetTextureUpdateCallback(
    std::function<void(int64_t, int64_t, int64_t)> callback) {
  thread_pool_ref_->Post([this, callback = std::move(callback)]() {
    if (destroyed_) {
      return;
    }
    texture_update_callback_ = callback;
    texture_update_callback_(texture_id_, GetVideoWidth(), GetVideoHeight());
  }).wait();
}

void VideoOutput::SetSize(std::optional<int64_t> width,
                          std::optional<int64_t> height) {
  thread_pool_ref_->Post([this, width, height]() {
    if (destroyed_) {
      return;
    }
    if (width.has_value()) {
      // H/W
      if (d3d11_renderer_ != nullptr) {
        width_ = width.value();
      }
      // S/W
      if (pixel_buffer_ != nullptr) {
        // Limit width if software rendering is being used.
        width_ = std::clamp(width.value(), static_cast<int64_t>(0),
                            static_cast<int64_t>(SW_RENDERING_MAX_WIDTH));
      }
    } else {
      width_ = std::nullopt;
    }
    if (height.has_value()) {
      // H/W
      if (d3d11_renderer_ != nullptr) {
        height_ = height.value();
      }
      // S/W
      if (pixel_buffer_ != nullptr) {
        // Limit width if software rendering is being used.
        height_ = std::clamp(height.value(), static_cast<int64_t>(0),
                             static_cast<int64_t>(SW_RENDERING_MAX_HEIGHT));
      }
    } else {
      height_ = std::nullopt;
    }
  });
}

void VideoOutput::CheckAndResize() {
  if (destroyed_) {
    return;
  }
  // Check if a new texture with different dimensions is needed.
  int64_t required_width = 0, required_height = 0;
  if (width_ && height_) {
    required_width = width_.value();
    required_height = height_.value();
  } else {
    GetVideoDimensions(required_width, required_height);
    if (width_) required_width = width_.value();
    if (height_) required_height = height_.value();
  }
  if (required_width < 1 || required_height < 1) {
    // Invalid.
    return;
  }
  int64_t current_width = -1, current_height = -1;
  if (d3d11_renderer_ != nullptr) {
    current_width = d3d11_renderer_->width();
    current_height = d3d11_renderer_->height();
  } else if (pixel_buffer_ != nullptr) {
    std::lock_guard<std::mutex> lock(texture_state_->mutex);
    auto it = texture_state_->pixel_buffer_textures.find(texture_id_);
    if (texture_id_ && it != texture_state_->pixel_buffer_textures.end() &&
        it->second != nullptr) {
      current_width = it->second->width;
      current_height = it->second->height;
    }
  }
  if (required_width == current_width && required_height == current_height) {
    // No creation of new texture required.
    return;
  }
  Resize(required_width, required_height);
}

void VideoOutput::Resize(int64_t required_width, int64_t required_height) {
  if (destroyed_) {
    return;
  }
  // Unregister previously registered texture & delete underlying objects.
  if (texture_id_) {
    Microsoft::WRL::ComPtr<ID3D11Texture2D> old_surface = nullptr;
    if (d3d11_renderer_ != nullptr) {
      old_surface = d3d11_renderer_->shared_texture();
    }
    registrar_->texture_registrar()->UnregisterTexture(
        texture_id_,
        [state = texture_state_, id = texture_id_,
         old_surface = std::move(old_surface)]() {
          std::lock_guard<std::mutex> lock(state->mutex);
          if (state->destroyed) {
            return;
          }
          state->texture_variants.erase(id);
          // H/W
          state->textures.erase(id);
          // S/W
          state->pixel_buffer_textures.erase(id);
        });
    texture_id_ = 0;
  }

  std::unique_ptr<flutter::TextureVariant> texture_variant;
  std::shared_ptr<FlutterDesktopGpuSurfaceDescriptor> gpu_texture;
  std::shared_ptr<FlutterDesktopPixelBuffer> sw_texture;

  if (d3d11_renderer_ != nullptr) {
    if (!d3d11_renderer_->SetSize(static_cast<int32_t>(required_width),
                                  static_cast<int32_t>(required_height))) {
      std::cout << "media_kit: VideoOutput: D3D11Renderer::SetSize failed."
                << std::endl;
      return;
    }

    gpu_texture = std::make_shared<FlutterDesktopGpuSurfaceDescriptor>();
    gpu_texture->struct_size = sizeof(FlutterDesktopGpuSurfaceDescriptor);
    gpu_texture->handle = d3d11_renderer_->shared_handle();
    gpu_texture->width = gpu_texture->visible_width = d3d11_renderer_->width();
    gpu_texture->height = gpu_texture->visible_height =
        d3d11_renderer_->height();
    gpu_texture->release_context = nullptr;
    gpu_texture->release_callback = [](void*) {};
    gpu_texture->format = kFlutterDesktopPixelFormatBGRA8888;

    texture_variant = std::make_unique<flutter::TextureVariant>(
        flutter::GpuSurfaceTexture(
            kFlutterDesktopGpuSurfaceTypeDxgiSharedHandle,
            [gpu_texture](size_t width, size_t height)
                -> const FlutterDesktopGpuSurfaceDescriptor* {
              return gpu_texture.get();
            }));
  } else if (pixel_buffer_ != nullptr) {
    sw_texture = std::make_shared<FlutterDesktopPixelBuffer>();
    sw_texture->buffer = pixel_buffer_.get();
    sw_texture->width = required_width;
    sw_texture->height = required_height;
    sw_texture->release_context = nullptr;
    sw_texture->release_callback = [](void*) {};

    texture_variant = std::make_unique<flutter::TextureVariant>(
        flutter::PixelBufferTexture(
            [sw_texture](size_t width, size_t height)
                -> const FlutterDesktopPixelBuffer* {
              return sw_texture.get();
            }));
  }

  if (!texture_variant) {
    return;
  }

  // Acquire lock before registration so Flutter callbacks see populated maps.
  {
    std::lock_guard<std::mutex> lock(texture_state_->mutex);
    texture_id_ =
        registrar_->texture_registrar()->RegisterTexture(texture_variant.get());
    if (gpu_texture) {
      texture_state_->textures.emplace(texture_id_, std::move(gpu_texture));
    }
    if (sw_texture) {
      texture_state_->pixel_buffer_textures.emplace(texture_id_,
                                                    std::move(sw_texture));
    }
    texture_state_->texture_variants.emplace(texture_id_,
                                             std::move(texture_variant));
  }

  // Notify public texture update callback outside the lock.
  texture_update_callback_(texture_id_, required_width, required_height);
}

void VideoOutput::GetVideoDimensions(int64_t& out_width, int64_t& out_height) {
  int64_t width = 0;
  int64_t height = 0;

  mpv_node params = {};
  if (mpv_get_property(handle_, "video-out-params", MPV_FORMAT_NODE, &params) >= 0) {
    int64_t dw = 0, dh = 0, rotate = 0;
    if (params.format == MPV_FORMAT_NODE_MAP) {
      for (int32_t i = 0; i < params.u.list->num; i++) {
        char* key = params.u.list->keys[i];
        auto value = params.u.list->values[i];
        if (value.format == MPV_FORMAT_INT64) {
          if (strcmp(key, "dw") == 0) {
            dw = value.u.int64;
          }
          if (strcmp(key, "dh") == 0) {
            dh = value.u.int64;
          }
          if (strcmp(key, "rotate") == 0) {
            rotate = value.u.int64;
          }
        }
      }
      mpv_free_node_contents(&params);
    }

    width = rotate == 0 || rotate == 180 ? dw : dh;
    height = rotate == 0 || rotate == 180 ? dh : dw;
  }

  if (pixel_buffer_ != nullptr && width > 0 && height > 0) {
    // Make sure |width| & |height| fit between |SW_RENDERING_MAX_WIDTH| &
    // |SW_RENDERING_MAX_HEIGHT| while maintaining aspect-ratio.
    double scale = (std::min)(
        1.0,
        (std::min)(static_cast<double>(SW_RENDERING_MAX_WIDTH) / width,
                   static_cast<double>(SW_RENDERING_MAX_HEIGHT) / height));
    width = std::clamp<int64_t>(
        static_cast<int64_t>(std::round(width * scale)), 1,
        SW_RENDERING_MAX_WIDTH);
    height = std::clamp<int64_t>(
        static_cast<int64_t>(std::round(height * scale)), 1,
        SW_RENDERING_MAX_HEIGHT);
  }

  out_width = width;
  out_height = height;
}

int64_t VideoOutput::GetVideoWidth() {
  // Fixed width.
  if (width_) {
    return width_.value();
  }
  int64_t width = 0;
  int64_t height = 0;
  GetVideoDimensions(width, height);
  return width;
}

int64_t VideoOutput::GetVideoHeight() {
  // Fixed height.
  if (height_) {
    return height_.value();
  }
  int64_t width = 0;
  int64_t height = 0;
  GetVideoDimensions(width, height);
  return height;
}
