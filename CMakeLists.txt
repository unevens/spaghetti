cmake_minimum_required(VERSION 4.1)

project(Spaghetti)

set(CMAKE_CXX_STANDARD            20)
set(CMAKE_CXX_STANDARD_REQUIRED   YES)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

add_subdirectory(webgpu)
add_compile_definitions(IMGUI_IMPL_WEBGPU_BACKEND_WGPU)
add_compile_definitions(IMGUI_DEFINE_MATH_OPERATORS)

file(GLOB spaghetti_src CONFIGURE_DEPENDS "src/*.h" "src/*.cpp")
file(GLOB node_editor CONFIGURE_DEPENDS "ImNodeFlow/include/*.h" "ImNodeFlow/src/*.cpp")
file(GLOB text_editor CONFIGURE_DEPENDS "ImGuiColorTextEdit/*.h" "ImGuiColorTextEdit/*.cpp")
file(GLOB imgui CONFIGURE_DEPENDS "imgui/*.h" "imgui/*.cpp")
file(GLOB timeline CONFIGURE_DEPENDS "ImTimeline/*.h" "ImTimeline/*.cpp" "ImTimeline/Core/*.h" "ImTimeline/Core/*.cpp" "ImTimeline/TimelineCore/*.h" "ImTimeline/TimelineCore/*.cpp" "ImTimeline/TimelineData/*.h" "ImTimeline/TimelineData/*.cpp" "ImTimeline/TimelineViews/*.h" "ImTimeline/TimelineViews/*.cpp")
set(imgui_backend imgui/backends/imgui_impl_glfw.h imgui/backends/imgui_impl_glfw.cpp imgui/backends/imgui_impl_wgpu.h imgui/backends/imgui_impl_wgpu.cpp)

add_executable(SpaghettiApp ${spaghetti_src} ${imgui} ${imgui_backend} ${node_editor} ${text_editor} ${timeline})
target_include_directories(SpaghettiApp PRIVATE "src" "imgui" "imgui/backends" "ImGuiColorTextEdit" "ImNodeFlow/include" "stb" "ImTimeline")

if (NOT EMSCRIPTEN)
    add_subdirectory(glfw)
else()
    add_library(glfw INTERFACE)
    target_link_options(glfw INTERFACE -sUSE_GLFW=3)
endif()

target_link_libraries(SpaghettiApp PRIVATE webgpu glfw)
target_copy_webgpu_binaries(SpaghettiApp)

if (XCODE)
    set_target_properties(SpaghettiApp PROPERTIES
        XCODE_GENERATE_SCHEME ON
        XCODE_SCHEME_ENABLE_GPU_FRAME_CAPTURE_MODE "Metal"
    )
endif()

# if (MSVC)
#     target_compile_options(SpaghettiApp PRIVATE /W4)
# else()
#     target_compile_options(SpaghettiApp PRIVATE -Wall -Wextra -pedantic)
# endif()