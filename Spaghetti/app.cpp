#include "App.h"

#include "imgui_impl_glfw.h"
#include "imgui_impl_wgpu.h"

#include <glfw3webgpu.h>
// #undef Status // X11 headers are leaking this and also 'Success', 'Always', 'None', all used in DAWN api.

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#if defined(IMGUI_IMPL_WEBGPU_BACKEND_WGPU)
#include <emscripten/html5_webgpu.h>
#endif
#include "../libs/emscripten/emscripten_mainloop_stub.h"
#endif

using namespace wgpu;

static void glfw_error_callback(int error, const char* description) {
  printf("GLFW Error %d: %s\n", error, description);
}
static float GetMainScale() {
  return ImGui_ImplGlfw_GetContentScaleForMonitor(glfwGetPrimaryMonitor());
}

#ifdef IMGUI_IMPL_WEBGPU_BACKEND_WPGU
static const char* GetLogLevelName(WGPULogLevel level) {
  switch (level) {
    case WGPULogLevel_Error:
      return "Error";
    case WGPULogLevel_Warn:
      return "Warn";
    case WGPULogLevel_Info:
      return "Info";
    case WGPULogLevel_Debug:
      return "Debug";
    case WGPULogLevel_Trace:
      return "Trace";
    default:
      return "Unknown";
  }
}
#endif

#ifdef IMGUI_IMPL_WEBGPU_BACKEND_DAWN

constexpr bool use_wgpu_limits = false;

static Limits GetRequiredLimits(Adapter adapter) {
  // Get adapter supported limits, in case we need them
  Limits supportedLimits;
  adapter.getLimits(&supportedLimits);

  // Don't forget to = Default
  Limits requiredLimits = Default;

  // We use at most 2 vertex attributes
  requiredLimits.maxVertexAttributes = 2;
  // We should also tell that we use 1 vertex buffers
  requiredLimits.maxVertexBuffers = 1;
  // Maximum size of a buffer is 15 vertices of 5 float each
  requiredLimits.maxBufferSize = 15 * 5 * sizeof(float);
  // Maximum stride between 2 consecutive vertices in the vertex buffer
  requiredLimits.maxVertexBufferArrayStride = 5 * sizeof(float);

  // There is a maximum of 3 float forwarded from vertex to fragment shader
  requiredLimits.maxInterStageShaderVariables = 3;

  // We use at most 1 bind group for now
  requiredLimits.maxBindGroups = 1;
  // We use at most 1 uniform buffer per stage
  requiredLimits.maxUniformBuffersPerShaderStage = 1;
  // Uniform structs have a size of maximum 16 float (more than what we need)
  requiredLimits.maxUniformBufferBindingSize = 16 * 4;

  // These two limits are different because they are "minimum" limits,
  // they are the only ones we are may forward from the adapter's supported
  // limits.
  requiredLimits.minUniformBufferOffsetAlignment = supportedLimits.minUniformBufferOffsetAlignment;
  requiredLimits.minStorageBufferOffsetAlignment = supportedLimits.minStorageBufferOffsetAlignment;
  return requiredLimits;
}

#endif

bool App::InitWGPU() {
  wgpu::InstanceDescriptor instance_desc = {};
  // instance_desc.features.timedWaitAnyEnable = true;
  // instance_desc.features.timedWaitAnyMaxCount = 1;
#ifdef __EMSCRIPTEN__
  wgpu_instance = { wgpuCreateInstance(nullptr) };
#else
  wgpu_instance = { wgpuCreateInstance(&instance_desc) };
#endif

#ifdef IMGUI_IMPL_WEBGPU_BACKEND_WPGU
  wgpuSetLogCallback(
    [](WGPULogLevel level, WGPUStringView msg, void* userdata) {
      fprintf(stderr, "%s: %.*s\n", GetLogLevelName(level), (int)msg.length, msg.data);
    },
    nullptr);
  wgpuSetLogLevel(WGPULogLevel_Warn);
#endif

  wgpu_surface = { glfwCreateWindowWGPUSurface(*wgpu_instance, window) };

  WGPUTextureFormat preferred_format = WGPUTextureFormat_Undefined;

#ifdef __EMSCRIPTEN__
  getAdapterAndDeviceViaJS();

  wgpu_device = emscripten_webgpu_get_device();
  IM_ASSERT(wgpu_device != nullptr && "Error creating the Device");

  WGPUSurfaceDescriptorFromCanvasHTMLSelector html_surface_desc = {};
  html_surface_desc.chain.sType = WGPUSType_SurfaceDescriptorFromCanvasHTMLSelector;
  html_surface_desc.selector = "#canvas";

  WGPUSurfaceDescriptor surface_desc = {};
  surface_desc.nextInChain = &html_surface_desc.chain;

  // Create the surface.
  wgpu_surface = { wgpuInstanceCreateSurface(*wgpu_instance, &surface_desc) };
  preferred_format = wgpuSurfaceGetPreferredFormat(*wgpu_surface, {} /* adapter */);
#else // __EMSCRIPTEN__

  RequestAdapterOptions adapterOpts = {};
  adapterOpts.compatibleSurface = *wgpu_surface;
  Adapter adapter = wgpu_instance->requestAdapter(adapterOpts);
  std::cout << "Got adapter: " << adapter << std::endl;
  // ImGui_ImplWGPU_DebugPrintAdapterInfo(adapter);

  // instance.release();

  std::cout << "Requesting device..." << std::endl;
  DeviceDescriptor deviceDesc = {};
  deviceDesc.label = { "My Device", WGPU_STRLEN };
  deviceDesc.requiredFeatureCount = 0;
  deviceDesc.defaultQueue.label = { "The default queue", WGPU_STRLEN };
  deviceDesc.deviceLostCallbackInfo.callback = [](WGPUDevice const* device,
                                                  WGPUDeviceLostReason reason,
                                                  WGPUStringView message,
                                                  WGPU_NULLABLE void* userdata1,
                                                  WGPU_NULLABLE void* userdata2) {
    std::cout << "Device lost: reason " << reason;
    if (message.data && message.length) {
      std::cout << " (" << std::string(message.data, message.length) << ")";
    }
    std::cout << std::endl;
  };
  deviceDesc.uncapturedErrorCallbackInfo.callback = [](WGPUDevice const* device,
                                                       WGPUErrorType type,
                                                       WGPUStringView message,
                                                       WGPU_NULLABLE void* userdata1,
                                                       WGPU_NULLABLE void* userdata2) {
    std::cout << "Uncaptured device error: type " << type;
    std::cout << " (" << std::string(message.data, message.length) << ")";
    std::cout << std::endl;
  };

#ifdef IMGUI_IMPL_WEBGPU_BACKEND_DAWN
  if constexpr (use_wgpu_limits) {
    Limits requiredLimits = GetRequiredLimits(adapter);
    deviceDesc.requiredLimits = &requiredLimits;
  }
#endif

  wgpu_device = { adapter.requestDevice(deviceDesc) };
  std::cout << "Got device: " << *wgpu_device << std::endl;

  SurfaceCapabilities capabilities{};
  wgpu_surface->getCapabilities(adapter, &capabilities);
  preferred_format = capabilities.formats[0];

#endif

  wgpu_queue = { wgpu_device->getQueue() };

  wgpu_surface_configuration.width = surfaceWidth;
  wgpu_surface_configuration.height = surfaceHeight;
  wgpu_surface_configuration.usage = TextureUsage::RenderAttachment;
  if (capabilities.formatCount > 0)
    wgpu_surface_configuration.format = preferred_format;
  wgpu_surface_configuration.viewFormatCount = 0;
  wgpu_surface_configuration.viewFormats = nullptr;
  wgpu_surface_configuration.device = *wgpu_device;
  wgpu_surface_configuration.presentMode = PresentMode::Fifo;
  wgpu_surface_configuration.alphaMode = CompositeAlphaMode::Auto;
  wgpu_surface->configure(wgpu_surface_configuration);

  adapter.release();

  return true;
}

App& App::Get() {
  static App app{};
  return app;
}

App::App()
  : clearColor{ { 0.0, 0.0, 0.0, 1.0 } } {
  glfwSetErrorCallback(glfw_error_callback);
}

bool App::IsIconified() const {
  return glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0;
}

void App::CreateWindowAndStartMainLoop(std::function<void()> lambda) {
  CreateMainWindow();
  SetAppUiLoop(std::move(lambda));
  StartMainLoop();
  DestroyMainWindow();
}

bool App::CreateAndShowWindow() {
  float main_scale = GetMainScale();
  surfaceWidth *= main_scale;
  surfaceHeight *= main_scale;
  window = glfwCreateWindow(surfaceWidth, surfaceHeight, "Spaghetti", nullptr, nullptr);
  if (window == nullptr)
    return false;

  if (!InitWGPU()) {
    glfwDestroyWindow(window);
    glfwTerminate();
    return false;
  }
  glfwShowWindow(window);
  return true;
}

void App::SetupBackends() {
  // Setup Platform/Renderer backends
  ImGui_ImplGlfw_InitForOther(window, true);
#ifdef __EMSCRIPTEN__
  ImGui_ImplGlfw_InstallEmscriptenCallbacks(window, "#canvas");
#endif
  ImGui_ImplWGPU_InitInfo init_info;
  init_info.Device = *wgpu_device;
  init_info.NumFramesInFlight = 3;
  init_info.RenderTargetFormat = wgpu_surface_configuration.format;
  init_info.DepthStencilFormat = WGPUTextureFormat_Undefined;
  ImGui_ImplWGPU_Init(&init_info);
}

void App::Terminate() {
  glfwDestroyWindow(window);
}

bool App::CreateMainWindow() {
  glfwSetErrorCallback(glfw_error_callback);
  if (!glfwInit())
    return false;

  // Make sure GLFW does not initialize any graphics context.
  // This needs to be done explicitly later.
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  CreateAndShowWindow();

  // Setup Dear ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  (void)io;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls

  // Setup Dear ImGui style
  ImGui::StyleColorsDark();

  // Setup scaling
  ImGuiStyle& style = ImGui::GetStyle();
  auto const main_scale = GetMainScale();
  style.ScaleAllSizes(main_scale); // Bake a fixed style scale. (until we have a solution for dynamic style scaling,
                                   // changing this requires resetting Style + calling this again)
  style.FontScaleDpi = main_scale; // Set initial font scale. (using io.ConfigDpiScaleFonts=true makes this unnecessary.
                                   // We leave both here for documentation purpose)

  SetupBackends();

  // Load Fonts
  // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use
  // ImGui::PushFont()/PopFont() to select them.
  // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
  // - If the file cannot be loaded, the function will return a nullptr. Please handle those errors in your application
  // (e.g. use an assertion, or display an error and quit).
  // - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use Freetype for higher quality font rendering.
  // - Read 'docs/FONTS.md' for more instructions and details. If you like the default font but want it to scale better,
  // consider using the 'ProggyVector' from the same author!
  // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double
  // backslash \\ !
  // - Our Emscripten build process allows embedding fonts to be accessible at runtime from the "fonts/" folder. See
  // Makefile.emscripten for details.
  // style.FontSizeBase = 20.0f;
  // io.Fonts->AddFontDefault();
#ifndef IMGUI_DISABLE_FILE_FUNCTIONS
  // io.Fonts->AddFontFromFileTTF("fonts/segoeui.ttf");
  // io.Fonts->AddFontFromFileTTF("fonts/DroidSans.ttf");
  // io.Fonts->AddFontFromFileTTF("fonts/Roboto-Medium.ttf");
  // io.Fonts->AddFontFromFileTTF("fonts/Cousine-Regular.ttf");
  // ImFont* font = io.Fonts->AddFontFromFileTTF("fonts/ArialUni.ttf");
  // IM_ASSERT(font != nullptr);
#endif
  return true;
}

void App::StartMainLoop() {
  // Main loop
#ifdef __EMSCRIPTEN__
  // For an Emscripten build we are disabling file-system access, so let's not attempt to do a fopen() of the imgui.ini
  // file. You may manually call LoadIniSettingsFromMemory() to load settings from your own storage.
  io.IniFilename = nullptr;
  EMSCRIPTEN_MAINLOOP_BEGIN
#else
  while (!WindowShouldClose())
#endif
  {
    // Poll and handle events (inputs, window resize, etc.)
    // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your
    // inputs.
    // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite
    // your copy of the mouse data.
    // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or
    // clear/overwrite your copy of the keyboard data. Generally you may always pass all inputs to dear imgui, and hide
    // them from your application based on those two flags.
    glfwPollEvents();
    if (IsIconified()) {
      ImGui_ImplGlfw_Sleep(10);
      continue;
    }

    // React to changes in screen size
    int width, height;
    glfwGetFramebufferSize((GLFWwindow*)window, &width, &height);
    if (width != surfaceWidth || height != surfaceHeight)
      ResizeSurface(width, height);

    // Check surface status for error. If texture is not optimal, try to reconfigure the surface.
    WGPUSurfaceTexture surface_texture;
    wgpuSurfaceGetCurrentTexture(*wgpu_surface, &surface_texture);

    if (ImGui_ImplWGPU_IsSurfaceStatusError(surface_texture.status)) {
      fprintf(stderr, "Unrecoverable Surface Texture status=%#.8x\n", surface_texture.status);
      abort();
    }
    if (ImGui_ImplWGPU_IsSurfaceStatusSubOptimal(surface_texture.status)) {
      if (surface_texture.texture)
        wgpuTextureRelease(surface_texture.texture);
      if (width > 0 && height > 0)
        ResizeSurface(width, height);
      continue;
    }

    // Start the Dear ImGui frame
    ImGui_ImplWGPU_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // App Loop
    if (appUiLoop)
      appUiLoop();

    // Rendering
    ImGui::Render();

    WGPUTextureViewDescriptor view_desc = {};
    view_desc.format = wgpu_surface_configuration.format;
    view_desc.dimension = WGPUTextureViewDimension_2D;
    view_desc.mipLevelCount = WGPU_MIP_LEVEL_COUNT_UNDEFINED;
    view_desc.arrayLayerCount = WGPU_ARRAY_LAYER_COUNT_UNDEFINED;
    view_desc.aspect = WGPUTextureAspect_All;

    WGPUTextureView texture_view = wgpuTextureCreateView(surface_texture.texture, &view_desc);

    WGPURenderPassColorAttachment color_attachments = {};
    color_attachments.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
    color_attachments.loadOp = WGPULoadOp_Clear;
    color_attachments.storeOp = WGPUStoreOp_Store;
    color_attachments.clearValue = {
      clearColor[0] * clearColor[3], clearColor[1] * clearColor[3], clearColor[2] * clearColor[3], clearColor[3]
    };
    color_attachments.view = texture_view;

    WGPURenderPassDescriptor render_pass_desc = {};
    render_pass_desc.colorAttachmentCount = 1;
    render_pass_desc.colorAttachments = &color_attachments;
    render_pass_desc.depthStencilAttachment = nullptr;

    WGPUCommandEncoderDescriptor enc_desc = {};
    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(*wgpu_device, &enc_desc);

    WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(encoder, &render_pass_desc);
    ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), pass);
    wgpuRenderPassEncoderEnd(pass);

    WGPUCommandBufferDescriptor cmd_buffer_desc = {};
    WGPUCommandBuffer cmd_buffer = wgpuCommandEncoderFinish(encoder, &cmd_buffer_desc);
    wgpuQueueSubmit(*wgpu_queue, 1, &cmd_buffer);

#ifndef __EMSCRIPTEN__
    wgpuSurfacePresent(*wgpu_surface);
    // Tick needs to be called in Dawn to display validation errors
// #if defined(IMGUI_IMPL_WEBGPU_BACKEND_DAWN)
//     wgpuDeviceTick(wgpu_device);
// #endif
#endif
    wgpuTextureViewRelease(texture_view);
    wgpuRenderPassEncoderRelease(pass);
    wgpuCommandEncoderRelease(encoder);
    wgpuCommandBufferRelease(cmd_buffer);
  }
#ifdef __EMSCRIPTEN__
  EMSCRIPTEN_MAINLOOP_END;
#endif
}

void App::DestroyMainWindow() {
  ImGui_ImplWGPU_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
  Terminate();
  glfwTerminate();
}

void App::SetAppUiLoop(std::function<void()> lambda) {
  this->appUiLoop = std::move(lambda);
}

void App::ResizeSurface(int width, int height) {
  wgpu_surface_configuration.width = surfaceWidth = width;
  wgpu_surface_configuration.height = surfaceHeight = height;
  wgpuSurfaceConfigure(*wgpu_surface, &wgpu_surface_configuration);
  for (auto& onResize : onWindowSizeChanged) {
    onResize(surfaceWidth, surfaceHeight);
  }
}

bool App::WindowShouldClose() {
  return glfwWindowShouldClose(window);
}
