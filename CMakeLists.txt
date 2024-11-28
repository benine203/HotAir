cmake_minimum_required(VERSION 3.15)

include(FetchContent)
include(GoogleTest)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT ENV{VCPKG_ROOT})
  FetchContent_Declare(vcpkg
                       GIT_REPOSITORY https://github.com/microsoft/vcpkg.git)
  FetchContent_MakeAvailable(vcpkg)
  if(NOT VCPKG_BOOTSTRAPPED)
    message(STATUS "Bootstrapping vcpkg")
    execute_process(COMMAND ${vcpkg_SOURCE_DIR}/bootstrap-vcpkg.sh)
    set(VCPKG_BOOTSTRAPPED
        ON
        CACHE BOOL "" FORCE)
  endif()
  set(ENV{VCPKG_ROOT} ${vcpkg_SOURCE_DIR})
endif()

set(CMAKE_TOOLCHAIN_FILE
    "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
    CACHE STRING "" FORCE)

project(HotAir LANGUAGES C CXX)

function(vcpkg_install)
  set(packages ${ARGV})
  list(POP_FRONT packages)
  cmake_parse_arguments(VCPKG "" "" "PACKAGES" ${packages})
  execute_process(COMMAND "$ENV{VCPKG_ROOT}/vcpkg" install --recurse
                          ${packages})
endfunction()

find_package(PkgConfig)

pkg_check_modules(WAYLAND wayland-client wayland-server)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(WAYLAND DEFAULT_MSG WAYLAND_LIBRARIES)

execute_process(
  COMMAND "sh" "-c"
          "pkg-config --variable=pkgdatadir wayland-protocols | tr -d '\n'"
  OUTPUT_VARIABLE WAYLAND_PROTOCOLS_DIR)
message(STATUS "WAYLAND_PROTOCOLS_DIR: ${WAYLAND_PROTOCOLS_DIR}")

# execute_process(COMMAND "wayland-scanner" private-code
# "${WAYLAND_PROTOCOLS_DIR}/stable/xdg-shell/xdg-shell.xml"
# "${CMAKE_CURRENT_BINARY_DIR}/xdg-shell-protocol.c") execute_process(COMMAND
# "wayland-scanner" client-header
# "${WAYLAND_PROTOCOLS_DIR}/stable/xdg-shell/xdg-shell.xml"
# "${CMAKE_CURRENT_BINARY_DIR}/xdg-shell-client-protocol.h")

add_custom_command(
  OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xdg-shell-protocol.c"
  COMMAND
    "wayland-scanner" "private-code"
    "${WAYLAND_PROTOCOLS_DIR}/stable/xdg-shell/xdg-shell.xml"
    "${CMAKE_CURRENT_BINARY_DIR}/xdg-shell-protocol.c"
  DEPENDS "${WAYLAND_PROTOCOLS_DIR}/stable/xdg-shell/xdg-shell.xml"
  COMMENT "Generating xdg-shell-protocol.c"
  VERBATIM)

add_custom_command(
  OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xdg-shell-client-protocol.h"
  COMMAND
    "wayland-scanner" "client-header"
    "${WAYLAND_PROTOCOLS_DIR}/stable/xdg-shell/xdg-shell.xml"
    "${CMAKE_CURRENT_BINARY_DIR}/xdg-shell-client-protocol.h"
  DEPENDS "${WAYLAND_PROTOCOLS_DIR}/stable/xdg-shell/xdg-shell.xml"
  COMMENT "Generating xdg-shell-client-protocol.h"
  VERBATIM)

  # xdg decoration manager
  add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xdg-decoration-protocol.c"
    COMMAND
      "wayland-scanner" "private-code"
      "${WAYLAND_PROTOCOLS_DIR}/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml"
      "${CMAKE_CURRENT_BINARY_DIR}/xdg-decoration-protocol.c"
    DEPENDS "${WAYLAND_PROTOCOLS_DIR}/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml"
    COMMENT "Generating xdg-decoration-protocol.c"
    VERBATIM)

add_custom_command(
  OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xdg-decoration-client-protocol.h"
  COMMAND
    "wayland-scanner" "client-header"
    "${WAYLAND_PROTOCOLS_DIR}/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml"
    "${CMAKE_CURRENT_BINARY_DIR}/xdg-decoration-client-protocol.h"
  DEPENDS "${WAYLAND_PROTOCOLS_DIR}/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml"
  COMMENT "Generating xdg-decoration-client-protocol.h"
  VERBATIM)

# add_custom_target(mywayland-protocols ALL DEPENDS
# "${CMAKE_CURRENT_BINARY_DIR}/xdg-shell-protocol.c"
# "${CMAKE_CURRENT_BINARY_DIR}/xdg-shell-client-protocol.h")
# add_library(mywayland-protocols STATIC
# "${CMAKE_CURRENT_BINARY_DIR}/xdg-shell-protocol.c")
# target_sources(mywayland-protocols PRIVATE
# "${CMAKE_CURRENT_BINARY_DIR}/xdg-shell-client-protocol.h")
# target_include_directories(mywayland-protocols PUBLIC
# "${CMAKE_CURRENT_BINARY_DIR}")

vcpkg_install(PACKAGES cpptrace gtest simdjson)

#find_path(SIMPLEINI_INCLUDE_DIRS "ConvertUTF.c")

find_package(simdjson CONFIG REQUIRED)

find_package(Vulkan REQUIRED)

# libdecor-0
pkg_check_modules(LIBDECOR libdecor-0)

if (NOT LIBDECOR_FOUND)
  message(FATAL_ERROR "libdecor-0 not found")
endif()

list(APPEND HotAIR_Includes ${LIBDECOR_INCLUDE_DIRS})

list(APPEND HotAIR_Includes ${WAYLAND_INCLUDE_DIRS} ${CMAKE_CURRENT_BINARY_DIR})
# list(APPEND HotAIR_Includes  ${SIMPLEINI_INCLUDE_DIRS})

list(APPEND HotAIR_Depends Vulkan::Vulkan)
list(APPEND HotAIR_Depends simdjson::simdjson)
list(APPEND HotAIR_Depends ${LIBDECOR_LIBRARIES})

list(APPEND HotAir_Sources main.cpp)

list(APPEND HotAir_Sources ${CMAKE_CURRENT_BINARY_DIR}/xdg-shell-protocol.c
                      ${CMAKE_CURRENT_BINARY_DIR}/xdg-shell-client-protocol.h
                      ${CMAKE_CURRENT_BINARY_DIR}/xdg-decoration-protocol.c
                      ${CMAKE_CURRENT_BINARY_DIR}/xdg-decoration-client-protocol.h)

add_executable(HotAir ${HotAir_Sources})

target_include_directories(HotAir PRIVATE ${HotAIR_Includes})

target_link_libraries(HotAir PRIVATE ${WAYLAND_LIBRARIES} ${HotAIR_Depends})

set(HOTAIR_TESTS ON CACHE BOOL "Build tests")

if (HOTAIR_TESTS)
  add_subdirectory(tests)
endif()