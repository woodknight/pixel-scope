cmake_minimum_required(VERSION 3.24)

project(PixelScope VERSION 0.1.2 LANGUAGES CXX C)

include(FetchContent)
include(CTest)
include(GNUInstallDirs)

if(WIN32)
  set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION ${CMAKE_INSTALL_BINDIR})
  set(CMAKE_INSTALL_UCRT_LIBRARIES TRUE)
  include(InstallRequiredSystemLibraries)
endif()

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

find_program(RAWLOADER_CARGO_EXECUTABLE NAMES cargo REQUIRED)
find_program(RAWLOADER_RUSTC_EXECUTABLE NAMES rustc REQUIRED)
find_program(RAWLOADER_RUSTUP_EXECUTABLE NAMES rustup)

if(RAWLOADER_RUSTUP_EXECUTABLE)
  execute_process(
    COMMAND ${RAWLOADER_RUSTUP_EXECUTABLE} which cargo
    OUTPUT_VARIABLE RAWLOADER_RUSTUP_CARGO_PATH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
    RESULT_VARIABLE RAWLOADER_RUSTUP_CARGO_RESULT
  )
  if(RAWLOADER_RUSTUP_CARGO_RESULT EQUAL 0 AND NOT RAWLOADER_RUSTUP_CARGO_PATH STREQUAL "")
    set(RAWLOADER_CARGO_EXECUTABLE "${RAWLOADER_RUSTUP_CARGO_PATH}")
  endif()

  execute_process(
    COMMAND ${RAWLOADER_RUSTUP_EXECUTABLE} which rustc
    OUTPUT_VARIABLE RAWLOADER_RUSTUP_RUSTC_PATH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
    RESULT_VARIABLE RAWLOADER_RUSTUP_RUSTC_RESULT
  )
  if(RAWLOADER_RUSTUP_RUSTC_RESULT EQUAL 0 AND NOT RAWLOADER_RUSTUP_RUSTC_PATH STREQUAL "")
    set(RAWLOADER_RUSTC_EXECUTABLE "${RAWLOADER_RUSTUP_RUSTC_PATH}")
  endif()
endif()

set(RAWLOADER_BRIDGE_TARGET_DIR "${CMAKE_CURRENT_BINARY_DIR}/rawloader_bridge-target")
set(RAWLOADER_BRIDGE_EXECUTABLE
    "${RAWLOADER_BRIDGE_TARGET_DIR}/release/rawloader_bridge${CMAKE_EXECUTABLE_SUFFIX}")
set(METADATA_BRIDGE_TARGET_DIR "${CMAKE_CURRENT_BINARY_DIR}/metadata_bridge-target")
set(METADATA_BRIDGE_EXECUTABLE
    "${METADATA_BRIDGE_TARGET_DIR}/release/metadata_bridge${CMAKE_EXECUTABLE_SUFFIX}")

add_custom_command(
  OUTPUT "${RAWLOADER_BRIDGE_EXECUTABLE}"
  COMMAND
    ${CMAKE_COMMAND} -E env
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

add_custom_command(
  OUTPUT "${METADATA_BRIDGE_EXECUTABLE}"
  COMMAND
    ${CMAKE_COMMAND} -E env
    "RUSTC=${RAWLOADER_RUSTC_EXECUTABLE}"
    "CARGO_TARGET_DIR=${METADATA_BRIDGE_TARGET_DIR}"
    "${RAWLOADER_CARGO_EXECUTABLE}" build --manifest-path
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/metadata_bridge/Cargo.toml" --release
  DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/metadata_bridge/Cargo.toml"
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/metadata_bridge/src/main.rs"
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  COMMENT "Building metadata bridge"
  VERBATIM
)

add_custom_target(metadata_bridge_build DEPENDS "${METADATA_BRIDGE_EXECUTABLE}")

add_library(pixelscope_core STATIC
  src/core/histogram.cpp
  src/core/image.cpp
  src/core/image_model.cpp
  src/core/image_statistics.cpp
  src/core/viewport.cpp
  src/io/binary_raw_loader.cpp
  src/io/dng_loader.cpp
  src/io/file_dialog.cpp
  src/io/image_loader.cpp
  src/io/metadata_loader.cpp
  src/platform/runtime_paths.cpp
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
          PIXELSCOPE_RAWLOADER_BRIDGE_FILENAME="rawloader_bridge${CMAKE_EXECUTABLE_SUFFIX}"
          PIXELSCOPE_METADATA_BRIDGE_PATH="${METADATA_BRIDGE_EXECUTABLE}"
          PIXELSCOPE_METADATA_BRIDGE_FILENAME="metadata_bridge${CMAKE_EXECUTABLE_SUFFIX}"
)
target_link_libraries(pixelscope_core PUBLIC SDL2::SDL2 imgui_lib tinyfiledialogs_lib TIFF::TIFF)
add_dependencies(pixelscope_core rawloader_bridge_build metadata_bridge_build)

add_executable(pixelscope src/main.cpp)
target_link_libraries(pixelscope PRIVATE pixelscope_core)

set(PIXELSCOPE_SHARED_LIBRARY_DESTINATION ${CMAKE_INSTALL_LIBDIR})
if(WIN32)
  set(PIXELSCOPE_SHARED_LIBRARY_DESTINATION ${CMAKE_INSTALL_BINDIR})
elseif(APPLE)
  set(PIXELSCOPE_SHARED_LIBRARY_DESTINATION Frameworks)
endif()

if(APPLE)
  set_target_properties(pixelscope PROPERTIES
    INSTALL_RPATH "@executable_path/../Frameworks"
  )
elseif(UNIX)
  set_target_properties(pixelscope PROPERTIES
    INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}"
  )
endif()

if(WIN32)
  target_link_libraries(pixelscope PRIVATE SDL2::SDL2main)
  add_custom_command(TARGET pixelscope POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      $<TARGET_RUNTIME_DLLS:pixelscope>
      $<TARGET_FILE_DIR:pixelscope>
    COMMAND_EXPAND_LISTS
  )
endif()

if(PIXELSCOPE_BUILD_TESTS)
  add_executable(pixelscope_tests tests/core_tests.cpp)
  target_compile_definitions(
    pixelscope_tests
    PRIVATE PIXELSCOPE_TEST_IMAGES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/images"
  )
  target_link_libraries(pixelscope_tests PRIVATE pixelscope_core)
  if(WIN32)
    add_custom_command(TARGET pixelscope_tests POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_RUNTIME_DLLS:pixelscope_tests>
        $<TARGET_FILE_DIR:pixelscope_tests>
      COMMAND_EXPAND_LISTS
    )
  endif()
  add_test(NAME pixelscope_tests COMMAND pixelscope_tests)
endif()

if(WIN32)
  install(TARGETS pixelscope
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    BUNDLE DESTINATION .
  )
else()
  install(TARGETS pixelscope
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    BUNDLE DESTINATION .
  )
endif()

install(TARGETS SDL2
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${PIXELSCOPE_SHARED_LIBRARY_DESTINATION}
  FRAMEWORK DESTINATION ${PIXELSCOPE_SHARED_LIBRARY_DESTINATION}
)

set(PIXELSCOPE_TIFF_RUNTIME_TARGET "")
if(TARGET TIFF::tiff)
  set(PIXELSCOPE_TIFF_RUNTIME_TARGET TIFF::tiff)
elseif(TARGET TIFF::TIFF)
  set(PIXELSCOPE_TIFF_RUNTIME_TARGET TIFF::TIFF)
endif()

set(PIXELSCOPE_TIFF_RUNTIME_FILES "")
if(PIXELSCOPE_TIFF_RUNTIME_TARGET)
  foreach(config_prop
      IMPORTED_LOCATION
      IMPORTED_LOCATION_RELEASE
      IMPORTED_LOCATION_RELWITHDEBINFO
      IMPORTED_LOCATION_MINSIZEREL
      IMPORTED_LOCATION_DEBUG)
    get_target_property(PIXELSCOPE_TIFF_RUNTIME_FILE ${PIXELSCOPE_TIFF_RUNTIME_TARGET} ${config_prop})
    if(PIXELSCOPE_TIFF_RUNTIME_FILE AND EXISTS "${PIXELSCOPE_TIFF_RUNTIME_FILE}")
      list(APPEND PIXELSCOPE_TIFF_RUNTIME_FILES "${PIXELSCOPE_TIFF_RUNTIME_FILE}")
    endif()
  endforeach()
  list(REMOVE_DUPLICATES PIXELSCOPE_TIFF_RUNTIME_FILES)
endif()

if(PIXELSCOPE_TIFF_RUNTIME_FILES)
  install(FILES ${PIXELSCOPE_TIFF_RUNTIME_FILES}
    DESTINATION ${PIXELSCOPE_SHARED_LIBRARY_DESTINATION}
  )
endif()

install(PROGRAMS
  "${RAWLOADER_BRIDGE_EXECUTABLE}"
  "${METADATA_BRIDGE_EXECUTABLE}"
  DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(WIN32)
  install(CODE [[
    file(GET_RUNTIME_DEPENDENCIES
      RESOLVED_DEPENDENCIES_VAR pixelscope_resolved_runtime_deps
      UNRESOLVED_DEPENDENCIES_VAR pixelscope_unresolved_runtime_deps
      EXECUTABLES
        "$<TARGET_FILE:pixelscope>"
        "]] "${RAWLOADER_BRIDGE_EXECUTABLE}" [["
        "]] "${METADATA_BRIDGE_EXECUTABLE}" [["
      DIRECTORIES
        "$<TARGET_FILE_DIR:pixelscope>"
        "]] "${RAWLOADER_BRIDGE_TARGET_DIR}/release" [["
        "]] "${METADATA_BRIDGE_TARGET_DIR}/release" [["
      PRE_EXCLUDE_REGEXES
        "api-ms-"
        "ext-ms-"
        ".*[Hh][Vv][Ss][Ii][Ff]ile[Tt]rust\\.dll"
        ".*[Ww][Tt][Dd][Cc][Cc][Mm]\\.dll"
      POST_EXCLUDE_REGEXES
        ".*/[Ss]ystem32/.*"
    )

    if(pixelscope_unresolved_runtime_deps)
      list(JOIN pixelscope_unresolved_runtime_deps ", " pixelscope_unresolved_runtime_deps_message)
      message(FATAL_ERROR
        "Unresolved Windows runtime dependencies: ${pixelscope_unresolved_runtime_deps_message}"
      )
    endif()

    list(REMOVE_DUPLICATES pixelscope_resolved_runtime_deps)
    foreach(runtime_dependency IN LISTS pixelscope_resolved_runtime_deps)
      file(INSTALL
        DESTINATION "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}"
        TYPE SHARED_LIBRARY
        FILES "${runtime_dependency}"
      )
    endforeach()
  ]])
endif()

if(APPLE)
  install(CODE [[
    include(BundleUtilities)
    fixup_bundle(
      "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/pixelscope"
      ""
      "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR};${CMAKE_INSTALL_PREFIX}/Frameworks"
    )
  ]])
endif()

install(FILES
  LICENSE
  DESTINATION ${CMAKE_INSTALL_DATADIR}/pixelscope
)

install(FILES
  assets/icon/icon_128x128.png
  assets/icon/icon_256x256.png
  assets/icon/pixel-scope.ico
  assets/icon/pixelscope.icns
  DESTINATION ${CMAKE_INSTALL_DATADIR}/pixelscope/icons
)

set(CPACK_PACKAGE_NAME "PixelScope")
set(CPACK_PACKAGE_VENDOR "PixelScope")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Cross-platform pixel inspection desktop viewer")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "PixelScope")
set(CPACK_PACKAGE_EXECUTABLES "pixelscope" "PixelScope")
set(CPACK_STRIP_FILES ON)

if(WIN32)
  set(CPACK_NSIS_DISPLAY_NAME "PixelScope")
  set(CPACK_NSIS_PACKAGE_NAME "PixelScope")
  set(CPACK_NSIS_INSTALLED_ICON_NAME "bin\\\\pixelscope.exe")
  set(CPACK_NSIS_MUI_ICON "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon/pixel-scope.ico")
  set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
endif()

if(APPLE)
  set(CPACK_GENERATOR "ZIP")
elseif(WIN32)
  set(CPACK_GENERATOR "ZIP")
else()
  set(CPACK_GENERATOR "TGZ")
endif()

include(CPack)
