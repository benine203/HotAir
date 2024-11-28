
static_assert(__cplusplus >= 202002L, "C++20 required");

// #include <cpptrace.hpp>
#include <getopt.h>
#include <wayland-client-protocol.h>
#include <wayland-client.h>

#include <memory>

#include "args.hpp"
#include "src/waylandGfx.hpp"

int main(int argc, char **argv) {
  // cpptrace::register_terminate_handler();

  Args::parse(argc, argv);

  std::unique_ptr<PlatformGfx> gfx = std::make_unique<WaylandGfx>();

  gfx->init();

  gfx->platform_event_loop([]() { return true; });

  return 0;
}