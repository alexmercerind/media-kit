// This file is a part of media_kit
// (https://github.com/media-kit/media-kit).
//
// Copyright © 2021 & onwards, Hitesh Kumar Saini <saini123hitesh@gmail.com>.
// All rights reserved.
// Use of this source code is governed by MIT license that can be found in the
// LICENSE file.

#ifndef VIDEO_OUTPUT_H_
#define VIDEO_OUTPUT_H_

#include <mpv/client.h>
#include <mpv/render.h>
#include <mpv/render_d3d11.h>

#include <atomic>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>

#include <flutter/method_channel.h>
#include <flutter/plugin_registrar_windows.h>
#include <flutter/standard_method_codec.h>

#include "d3d11_renderer.h"
#include "thread_pool.h"

typedef struct _VideoOutputConfiguration {
  std::optional<int64_t> width;
  std::optional<int64_t> height;
  bool enable_hardware_acceleration;

  _VideoOutputConfiguration(std::optional<int64_t> width = std::nullopt,
                            std::optional<int64_t> height = std::nullopt,
                            bool enable_hardware_acceleration = true)
      : width(width),
        height(height),
        enable_hardware_acceleration(enable_hardware_acceleration) {}
} VideoOutputConfiguration;

class VideoOutput {
 public:
  VideoOutput(int64_t handle,
              VideoOutputConfiguration configuration,
              flutter::PluginRegistrarWindows* registrar,
              ThreadPool* thread_pool_ref);

  ~VideoOutput();

  void SetTextureUpdateCallback(
      std::function<void(int64_t, int64_t, int64_t)> callback);

  void SetSize(std::optional<int64_t> width, std::optional<int64_t> height);

 private:
  void NotifyRender();

  void Render();

  void SkipFrame();

  void CheckAndResize();

  void Resize(int64_t required_width, int64_t required_height);

  void GetVideoDimensions(int64_t& out_width, int64_t& out_height);

  int64_t GetVideoWidth();

  int64_t GetVideoHeight();

  struct TextureRegistryState {
    std::mutex mutex;
    bool destroyed = false;
    std::unordered_map<int64_t, std::unique_ptr<flutter::TextureVariant>>
        texture_variants;
    std::unordered_map<int64_t,
                       std::shared_ptr<FlutterDesktopGpuSurfaceDescriptor>>
        textures;
    std::unordered_map<int64_t, std::shared_ptr<FlutterDesktopPixelBuffer>>
        pixel_buffer_textures;
  };

  std::optional<int64_t> height_ = std::nullopt;
  std::optional<int64_t> width_ = std::nullopt;
  VideoOutputConfiguration configuration_ = VideoOutputConfiguration{};

  mpv_handle* handle_ = nullptr;
  mpv_render_context* render_context_ = nullptr;
  int64_t texture_id_ = 0;
  flutter::PluginRegistrarWindows* registrar_ = nullptr;
  ThreadPool* thread_pool_ref_ = nullptr;
  // Stop queued callbacks before draining and freeing their borrowed resources.
  std::atomic<bool> destroyed_{false};
  std::shared_ptr<TextureRegistryState> texture_state_ =
      std::make_shared<TextureRegistryState>();

  // H/W rendering.

  std::unique_ptr<D3D11Renderer> d3d11_renderer_ = nullptr;

  // S/W rendering.

  std::unique_ptr<uint8_t[]> pixel_buffer_ = nullptr;

  // Public notifier. This is called when a new texture is registered & texture
  // ID is changed. Only happens when video output resolution changes.
  std::function<void(int64_t, int64_t, int64_t)> texture_update_callback_ =
      [](int64_t, int64_t, int64_t) {};
};

#endif  // VIDEO_OUTPUT_H_
