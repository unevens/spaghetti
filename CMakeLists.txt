cmake_minimum_required(VERSION 4.1)

project(Spaghetti)

set(CMAKE_CXX_STANDARD            20)
set(CMAKE_CXX_STANDARD_REQUIRED   YES)
# set_property(GLOBAL PROPERTY USE_FOLDERS ON)

add_subdirectory(webgpu)
add_compile_definitions(IMGUI_IMPL_WEBGPU_BACKEND_WGPU)
add_compile_definitions(IMGUI_DEFINE_MATH_OPERATORS)

set(node_flow_dir "ImNodeFlow")
file(GLOB node_flow CONFIGURE_DEPENDS "${node_flow_dir}/include/*.h" "${node_flow_dir}/src/*.cpp")
source_group("${node_flow_dir}" FILES ${node_flow})

set(text_editor_dir "ImGuiColorTextEdit")
file(GLOB text_editor CONFIGURE_DEPENDS "${text_editor_dir}/*.h" "${text_editor_dir}/*.cpp")
source_group("${text_editor_dir}" FILES ${text_editor})

set(imgui_dir "imgui")
file(GLOB imgui CONFIGURE_DEPENDS "${imgui_dir}/*.h" "${imgui_dir}/*.cpp")
source_group("${imgui_dir}" FILES ${imgui})

set(node_editor_dir "imgui-node-editor")
file(GLOB node_editor CONFIGURE_DEPENDS "${node_editor_dir}/*.h" "${node_editor_dir}/*.cpp")
source_group("${node_editor_dir}" FILES ${node_editor})

set(timeline_dir "ImTimeline")
file(GLOB timeline CONFIGURE_DEPENDS "${timeline_dir}/*.h" "${timeline_dir}/*.cpp" "${timeline_dir}/Core/*.h" "${timeline_dir}/Core/*.cpp" "${timeline_dir}/TimelineCore/*.h" "${timeline_dir}/TimelineCore/*.cpp" "${timeline_dir}/TimelineData/*.h" "${timeline_dir}/TimelineData/*.cpp" "${timeline_dir}/TimelineViews/*.h" "${timeline_dir}/TimelineViews/*.cpp")
source_group("${timeline_dir}" FILES ${timeline})

set(imgui_backend_dir "imgui/backends")
set(imgui_backend "${imgui_backend_dir}/imgui_impl_glfw.h" "${imgui_backend_dir}/imgui_impl_glfw.cpp" "${imgui_backend_dir}/imgui_impl_wgpu.h" "${imgui_backend_dir}/imgui_impl_wgpu.cpp")
source_group("${imgui_backend_dir}" FILES ${imgui_backend})

set(spaghetti_dir "Spaghetti")
file(GLOB_RECURSE spaghetti CONFIGURE_DEPENDS
	${spaghetti_dir}/**.cpp
	${spaghetti_dir}/**/**.cpp	
	${spaghetti_dir}/**.h
	${spaghetti_dir}/**/**.h
	${spaghetti_dir}/**.hpp
	${spaghetti_dir}/**/**.hpp)
source_group("${spaghetti_dir}" FILES ${spaghetti})

add_executable(SpaghettiApp ${spaghetti} ${imgui} ${imgui_backend} ${node_editor} ${text_editor} ${timeline} ${node_flow})
target_include_directories(SpaghettiApp PRIVATE "${spaghetti_dir}" "${imgui_dir}" "${imgui_backend_dir}" "${text_editor_dir}" "${node_flow_dir}/include/" "stb" "${timeline_dir}" "${node_editor_dir}")

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

