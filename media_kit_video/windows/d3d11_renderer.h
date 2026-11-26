// This file is a part of media_kit
// (https://github.com/media-kit/media-kit).
//
// Copyright © 2021 & onwards, Hitesh Kumar Saini <saini123hitesh@gmail.com>.
// All rights reserved.
// Use of this source code is governed by MIT license that can be found in the
// LICENSE file.

#ifndef D3D11_RENDERER_H_
#define D3D11_RENDERER_H_

#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>
#include <cstdint>

// Manages a Direct3D 11 device and double-buffered render/shared textures.
// All methods must be called from the single render ThreadPool worker thread.
class D3D11Renderer {
 public:
  D3D11Renderer(int32_t width, int32_t height,
                IDXGIAdapter* flutter_adapter = nullptr);
  ~D3D11Renderer();

  ID3D11Device* device() const { return d3d_11_device_.Get(); }
  Microsoft::WRL::ComPtr<ID3D11Texture2D> render_target() const {
    return render_target_texture_;
  }
  Microsoft::WRL::ComPtr<ID3D11Texture2D> shared_texture() const {
    return shared_texture_;
  }
  HANDLE shared_handle() const { return shared_handle_; }
  int32_t width() const { return width_; }
  int32_t height() const { return height_; }

  bool SetSize(int32_t width, int32_t height);
  void Commit();

 private:
  bool CreateD3D11Device(IDXGIAdapter* flutter_adapter);
  bool CreateSurfaces();
  void ReleaseSurfaces();

  int32_t width_ = 1;
  int32_t height_ = 1;

  Microsoft::WRL::ComPtr<ID3D11Device> d3d_11_device_;
  Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3d_11_device_context_;
  Microsoft::WRL::ComPtr<ID3D11Texture2D> render_target_texture_;
  Microsoft::WRL::ComPtr<ID3D11Texture2D> shared_texture_;
  HANDLE shared_handle_ = nullptr;
};

#endif  // D3D11_RENDERER_H_
