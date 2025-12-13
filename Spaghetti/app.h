#pragma once

#include "webgpu/webgpu.hpp"
#include <memory>
#include <functional>
#include <array>


class App final {

public:
  static App & Get();

  bool CreateMainWindow();
  void StartMainLoop();
  void DestroyMainWindow();
  void SetAppUiLoop(std::function<void()> lambda);
  std::array<float,4> clearColor;

private:
  App ();

private:
  std::unique_ptr<struct AppImpl> impl;
  std::function<void()> appUiLoop;
};