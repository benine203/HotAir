cmake_minimum_required(VERSION 3.15)

include(FetchContent)
include(GoogleTest)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (NOT ENV{VCPKG_ROOT})
    FetchContent_Declare(
        vcpkg
        GIT_REPOSITORY https://github.com/microsoft/vcpkg.git)
    FetchContent_MakeAvailable(vcpkg)
    if (NOT VCPKG_BOOTSTRAPPED)
        message(STATUS "Bootstrapping vcpkg")
        execute_process(
        COMMAND ${vcpkg_SOURCE_DIR}/bootstrap-vcpkg.sh)
    set(VCPKG_BOOTSTRAPPED ON CACHE BOOL "" FORCE)
    endif()
    set(ENV{VCPKG_ROOT} ${vcpkg_SOURCE_DIR})
endif()

set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "" FORCE)

project(HotAir LANGUAGES C CXX)

function(vcpkg_install packages)
    execute_process(
        COMMAND "$ENV{VCPKG_ROOT}/vcpkg" install --recurse ${packages})
endfunction()

find_package(PkgConfig)

pkg_check_modules(WAYLAND wayland-client wayland-server)

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(WAYLAND DEFAULT_MSG WAYLAND_LIBRARIES)

execute_process(COMMAND "sh" "-c" "pkg-config --variable=pkgdatadir wayland-protocols | tr -d '\n'" OUTPUT_VARIABLE WAYLAND_PROTOCOLS_DIR)
message(STATUS "WAYLAND_PROTOCOLS_DIR: ${WAYLAND_PROTOCOLS_DIR}")

# execute_process(COMMAND "wayland-scanner" private-code "${WAYLAND_PROTOCOLS_DIR}/stable/xdg-shell/xdg-shell.xml" "${CMAKE_CURRENT_BINARY_DIR}/xdg-shell-protocol.c")
# execute_process(COMMAND "wayland-scanner" client-header "${WAYLAND_PROTOCOLS_DIR}/stable/xdg-shell/xdg-shell.xml" "${CMAKE_CURRENT_BINARY_DIR}/xdg-shell-client-protocol.h")

add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xdg-shell-protocol.c"
    COMMAND "wayland-scanner" "private-code" "${WAYLAND_PROTOCOLS_DIR}/stable/xdg-shell/xdg-shell.xml" "${CMAKE_CURRENT_BINARY_DIR}/xdg-shell-protocol.c"
    DEPENDS "${WAYLAND_PROTOCOLS_DIR}/stable/xdg-shell/xdg-shell.xml"
    COMMENT "Generating xdg-shell-protocol.c"
    VERBATIM)

add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/xdg-shell-client-protocol.h"
    COMMAND "wayland-scanner" "client-header" "${WAYLAND_PROTOCOLS_DIR}/stable/xdg-shell/xdg-shell.xml" "${CMAKE_CURRENT_BINARY_DIR}/xdg-shell-client-protocol.h"
    DEPENDS "${WAYLAND_PROTOCOLS_DIR}/stable/xdg-shell/xdg-shell.xml"
    COMMENT "Generating xdg-shell-client-protocol.h"
    VERBATIM)

#add_custom_target(mywayland-protocols ALL DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/xdg-shell-protocol.c" "${CMAKE_CURRENT_BINARY_DIR}/xdg-shell-client-protocol.h")
#add_library(mywayland-protocols STATIC "${CMAKE_CURRENT_BINARY_DIR}/xdg-shell-protocol.c")
#target_sources(mywayland-protocols PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/xdg-shell-client-protocol.h")
#target_include_directories(mywayland-protocols PUBLIC "${CMAKE_CURRENT_BINARY_DIR}")

vcpkg_install(cpptrace gtest)

find_package(Vulkan REQUIRED)

add_executable(HotAir main.cpp ${CMAKE_CURRENT_BINARY_DIR}/xdg-shell-protocol.c ${CMAKE_CURRENT_BINARY_DIR}/xdg-shell-client-protocol.h)
target_include_directories(HotAir PRIVATE ${WAYLAND_INCLUDE_DIRS} ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(HotAir PRIVATE ${WAYLAND_LIBRARIES} Vulkan::Vulkan)

