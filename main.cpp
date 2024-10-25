
static_assert(__cplusplus >= 202002L, "C++20 required");

#include <algorithm>
// #include <cpptrace.hpp>
#include <getopt.h>
#include <wayland-client-protocol.h>
#include <wayland-client.h>

#include <atomic>
#include <format>
#include <iostream>
#include <memory>
#include <numeric>
#include <string>
#include <vector>

#include "args.hpp"
#include "src/waylandGfx.hpp"

int main(int argc, char **argv) {
  // cpptrace::register_terminate_handler();

  Args::parse(argc, argv);

  auto gfx = std::make_unique<WaylandGfx>();
  gfx->init();

  auto *cb = wl_surface_frame(gfx->window->surface);
  static const auto frame_listener = wl_callback_listener{
      .done = [](void *data, wl_callback *cb, uint32_t time) {
        wl_callback_destroy(cb);
      }};

  while (wl_display_dispatch(*gfx->display) != -1 && !gfx->window->closed) {
    // do nothing
  }

  return 0;
}