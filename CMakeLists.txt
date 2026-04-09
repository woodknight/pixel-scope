cmake_minimum_required(VERSION 3.24)

project(PixelScope VERSION 0.1.0 LANGUAGES CXX C)

include(FetchContent)
include(CTest)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(PIXELSCOPE_BUILD_TESTS "Build PixelScope tests" ON)

FetchContent_Declare(
  sdl2
  GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
  GIT_TAG release-2.30.11
)

FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG v1.91.9b
)

FetchContent_Declare(
  stb
  GIT_REPOSITORY https://github.com/nothings/stb.git
  GIT_TAG master
)

FetchContent_Declare(
  tinyfiledialogs
  GIT_REPOSITORY https://github.com/native-toolkit/libtinyfiledialogs.git
  GIT_TAG master
)

FetchContent_MakeAvailable(sdl2 imgui stb tinyfiledialogs)

add_library(imgui_lib STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_sdlrenderer2.cpp
)
target_include_directories(imgui_lib PUBLIC
  ${imgui_SOURCE_DIR}
  ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui_lib PUBLIC SDL2::SDL2)

add_library(tinyfiledialogs_lib STATIC
  ${tinyfiledialogs_SOURCE_DIR}/tinyfiledialogs.c
)
target_include_directories(tinyfiledialogs_lib PUBLIC ${tinyfiledialogs_SOURCE_DIR})
if(APPLE)
  find_library(COCOA_FRAMEWORK Cocoa REQUIRED)
  target_link_libraries(tinyfiledialogs_lib PUBLIC ${COCOA_FRAMEWORK})
elseif(UNIX AND NOT APPLE)
  target_link_libraries(tinyfiledialogs_lib PUBLIC dl)
endif()

add_library(pixelscope_core STATIC
  src/core/histogram.cpp
  src/core/image.cpp
  src/core/image_model.cpp
  src/core/viewport.cpp
  src/io/file_dialog.cpp
  src/io/image_loader.cpp
  src/render/texture_cache.cpp
  src/ui/app.cpp
)
target_include_directories(pixelscope_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${stb_SOURCE_DIR}
)
target_link_libraries(pixelscope_core PUBLIC SDL2::SDL2 imgui_lib tinyfiledialogs_lib)

add_executable(pixelscope src/main.cpp)
target_link_libraries(pixelscope PRIVATE pixelscope_core)

if(WIN32)
  target_link_libraries(pixelscope PRIVATE SDL2::SDL2main)
endif()

if(PIXELSCOPE_BUILD_TESTS)
  add_executable(pixelscope_tests tests/core_tests.cpp)
  target_link_libraries(pixelscope_tests PRIVATE pixelscope_core)
  add_test(NAME pixelscope_tests COMMAND pixelscope_tests)
endif()
