cmake_minimum_required(VERSION 3.24)

project(PixelScope VERSION 0.1.0 LANGUAGES CXX C)

include(FetchContent)
include(CTest)

find_package(TIFF REQUIRED)

if(POLICY CMP0169)
  cmake_policy(SET CMP0169 OLD)
endif()

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

execute_process(
  COMMAND rustup which cargo
  OUTPUT_VARIABLE RAWLOADER_CARGO_EXECUTABLE
  OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_QUIET
  RESULT_VARIABLE RAWLOADER_CARGO_RESULT
)
if(NOT RAWLOADER_CARGO_RESULT EQUAL 0 OR RAWLOADER_CARGO_EXECUTABLE STREQUAL "")
  find_program(RAWLOADER_CARGO_EXECUTABLE cargo REQUIRED)
endif()

execute_process(
  COMMAND rustup which rustc
  OUTPUT_VARIABLE RAWLOADER_RUSTC_EXECUTABLE
  OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_QUIET
  RESULT_VARIABLE RAWLOADER_RUSTC_RESULT
)
if(NOT RAWLOADER_RUSTC_RESULT EQUAL 0 OR RAWLOADER_RUSTC_EXECUTABLE STREQUAL "")
  find_program(RAWLOADER_RUSTC_EXECUTABLE rustc REQUIRED)
endif()

get_filename_component(RAWLOADER_RUST_BIN_DIR "${RAWLOADER_RUSTC_EXECUTABLE}" DIRECTORY)
set(RAWLOADER_BRIDGE_TARGET_DIR "${CMAKE_CURRENT_BINARY_DIR}/rawloader_bridge-target")
set(RAWLOADER_BRIDGE_EXECUTABLE
    "${RAWLOADER_BRIDGE_TARGET_DIR}/release/rawloader_bridge${CMAKE_EXECUTABLE_SUFFIX}")

add_custom_command(
  OUTPUT "${RAWLOADER_BRIDGE_EXECUTABLE}"
  COMMAND
    ${CMAKE_COMMAND} -E env
    "PATH=${RAWLOADER_RUST_BIN_DIR}:$ENV{PATH}"
    "RUSTC=${RAWLOADER_RUSTC_EXECUTABLE}"
    "CARGO_TARGET_DIR=${RAWLOADER_BRIDGE_TARGET_DIR}"
    "${RAWLOADER_CARGO_EXECUTABLE}" build --manifest-path
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/rawloader_bridge/Cargo.toml" --release
  DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/rawloader_bridge/Cargo.toml"
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/rawloader_bridge/src/main.rs"
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  COMMENT "Building rawloader bridge"
  VERBATIM
)

add_custom_target(rawloader_bridge_build DEPENDS "${RAWLOADER_BRIDGE_EXECUTABLE}")

add_library(pixelscope_core STATIC
  src/core/histogram.cpp
  src/core/image.cpp
  src/core/image_model.cpp
  src/core/viewport.cpp
  src/io/dng_loader.cpp
  src/io/file_dialog.cpp
  src/io/image_loader.cpp
  src/io/tiff_loader.cpp
  src/render/texture_cache.cpp
  src/ui/app.cpp
)
target_include_directories(pixelscope_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${stb_SOURCE_DIR}
)
target_compile_definitions(
  pixelscope_core
  PRIVATE PIXELSCOPE_RAWLOADER_BRIDGE_PATH="${RAWLOADER_BRIDGE_EXECUTABLE}"
)
target_link_libraries(pixelscope_core PUBLIC SDL2::SDL2 imgui_lib tinyfiledialogs_lib TIFF::TIFF)
add_dependencies(pixelscope_core rawloader_bridge_build)

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
