// This file is a part of media_kit
// (https://github.com/media-kit/media-kit).
//
// Copyright © 2021 & onwards, Hitesh Kumar Saini <saini123hitesh@gmail.com>.
// All rights reserved.
// Use of this source code is governed by MIT license that can be found in the
// LICENSE file.

#include "d3d11_renderer.h"

#include <iostream>
#include <iterator>
#include <stdexcept>

D3D11Renderer::D3D11Renderer(int32_t width, int32_t height,
                             IDXGIAdapter* flutter_adapter)
    : width_(width > 0 ? width : 1), height_(height > 0 ? height : 1) {
  if (!CreateD3D11Device(flutter_adapter)) {
    throw std::runtime_error("Unable to create Direct3D 11 device.");
  }
  if (!CreateSurfaces()) {
    throw std::runtime_error("Unable to create Direct3D 11 surfaces.");
  }
}

D3D11Renderer::~D3D11Renderer() {
  ReleaseSurfaces();
  d3d_11_device_context_.Reset();
  d3d_11_device_.Reset();
}

bool D3D11Renderer::CreateD3D11Device(IDXGIAdapter* flutter_adapter) {
  const D3D_FEATURE_LEVEL feature_levels[] = {
      D3D_FEATURE_LEVEL_11_1,
      D3D_FEATURE_LEVEL_11_0,
      D3D_FEATURE_LEVEL_10_1,
      D3D_FEATURE_LEVEL_10_0,
      D3D_FEATURE_LEVEL_9_3,
  };

  Microsoft::WRL::ComPtr<IDXGIAdapter> adapter = flutter_adapter;
  D3D_DRIVER_TYPE driver_type =
      adapter ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE;

  HRESULT hr = ::D3D11CreateDevice(
      adapter.Get(), driver_type, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
      feature_levels, static_cast<UINT>(std::size(feature_levels)),
      D3D11_SDK_VERSION, d3d_11_device_.GetAddressOf(), nullptr,
      d3d_11_device_context_.GetAddressOf());

  if (FAILED(hr)) {
    std::cout << "media_kit: D3D11Renderer: D3D11CreateDevice failed (hr=0x"
              << std::hex << hr << std::dec << ")" << std::endl;
    return false;
  }

  Microsoft::WRL::ComPtr<IDXGIDevice> dxgi_device;
  if (SUCCEEDED(d3d_11_device_->QueryInterface(__uuidof(IDXGIDevice),
                                               (void**)&dxgi_device)) &&
      dxgi_device) {
    dxgi_device->SetGPUThreadPriority(5);
  }

  return true;
}

bool D3D11Renderer::CreateSurfaces() {
  D3D11_TEXTURE2D_DESC desc = {};
  desc.Width = static_cast<UINT>(width_);
  desc.Height = static_cast<UINT>(height_);
  desc.MipLevels = 1;
  desc.ArraySize = 1;
  desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
  desc.SampleDesc.Count = 1;
  desc.Usage = D3D11_USAGE_DEFAULT;
  desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

  // 1. Private render target texture that mpv renders into.
  HRESULT hr =
      d3d_11_device_->CreateTexture2D(&desc, nullptr, &render_target_texture_);
  if (FAILED(hr)) {
    std::cout << "media_kit: D3D11Renderer: CreateTexture2D (render target) failed: 0x"
              << std::hex << hr << std::dec << std::endl;
    return false;
  }

  // 2. Shared texture exposed to Flutter via DXGI shared handle.
  desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;
  hr = d3d_11_device_->CreateTexture2D(&desc, nullptr, &shared_texture_);
  if (FAILED(hr)) {
    std::cout << "media_kit: D3D11Renderer: CreateTexture2D (shared) failed: 0x"
              << std::hex << hr << std::dec << std::endl;
    ReleaseSurfaces();
    return false;
  }

  Microsoft::WRL::ComPtr<IDXGIResource> resource;
  hr = shared_texture_.As(&resource);
  if (FAILED(hr) || FAILED(resource->GetSharedHandle(&shared_handle_)) ||
      shared_handle_ == nullptr) {
    std::cout << "media_kit: D3D11Renderer: GetSharedHandle failed: 0x"
              << std::hex << hr << std::dec << std::endl;
    ReleaseSurfaces();
    return false;
  }

  // Clear surfaces to black immediately to avoid uninitialized frame 0 noise.
  const FLOAT black[4] = {0.0f, 0.0f, 0.0f, 1.0f};
  for (ID3D11Texture2D* tex :
       {render_target_texture_.Get(), shared_texture_.Get()}) {
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv;
    if (SUCCEEDED(d3d_11_device_->CreateRenderTargetView(tex, nullptr, &rtv))) {
      d3d_11_device_context_->ClearRenderTargetView(rtv.Get(), black);
    }
  }

  d3d_11_device_context_->Flush();
  return true;
}

void D3D11Renderer::ReleaseSurfaces() {
  render_target_texture_.Reset();
  shared_texture_.Reset();
  shared_handle_ = nullptr;
}

bool D3D11Renderer::SetSize(int32_t width, int32_t height) {
  int32_t target_width = (width > 0) ? width : 1;
  int32_t target_height = (height > 0) ? height : 1;
  if (target_width == width_ && target_height == height_ &&
      render_target_texture_ && shared_texture_ && shared_handle_) {
    return true;
  }
  int32_t prev_width = width_;
  int32_t prev_height = height_;
  width_ = target_width;
  height_ = target_height;
  ReleaseSurfaces();
  if (!CreateSurfaces()) {
    width_ = prev_width;
    height_ = prev_height;
    return false;
  }
  return true;
}

void D3D11Renderer::Commit() {
  if (d3d_11_device_context_ && render_target_texture_ && shared_texture_) {
    d3d_11_device_context_->CopyResource(shared_texture_.Get(),
                                         render_target_texture_.Get());
    d3d_11_device_context_->Flush();
  }
}
