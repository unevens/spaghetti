/*
 * Part of Spaghetti.
 * Copyright 2025 Dario Mambro.
 * Distriuted under the GNU Affero General Public License.
 */

#pragma once

#include "webgpu/webgpu-raii.hpp"
#include <GLFW/glfw3.h>

#include <array>
#include <functional>
#include <memory>
#include <vector>

class App final {

public:
  static App& Get();

  bool CreateMainWindow();
  void StartMainLoop();
  void DestroyMainWindow();
  void SetAppUiLoop(std::function<void()> lambda);
  wgpu::Device GetDevice() { return *wgpu_device; }
  wgpu::Queue GetQueue() { return *wgpu_queue; }
  bool IsIconified() const;

  void CreateWindowAndStartMainLoop(std::function<void()> lambda);

public:
  std::array<float, 4> clearColor;
  std::vector<std::function<void(int, int)>> onWindowSizeChanged;

private:
  App();

  bool InitWGPU();
  void ResizeSurface(int width, int height);
  bool WindowShouldClose();
  bool CreateAndShowWindow();
  void SetupBackends();
  void Terminate();

private:
  int surfaceWidth = 1280;
  int surfaceHeight = 800;
  std::function<void()> appUiLoop;

private:
  wgpu::raii::Instance wgpu_instance{};
  wgpu::raii::Device wgpu_device{};
  wgpu::raii::Surface wgpu_surface{};
  wgpu::raii::Queue wgpu_queue{};
  wgpu::SurfaceConfiguration wgpu_surface_configuration{};

private:
  GLFWwindow* window = nullptr;
};

inline wgpu::Device Gpu() {
  return App::Get().GetDevice();
}

inline wgpu::Queue GpuQueue() {
  return App::Get().GetQueue();
}
