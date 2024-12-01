
static_assert(__cplusplus >= 202002L, "C++20 required");

#include <cpptrace.hpp>
#include <getopt.h>
#include <wayland-client-protocol.h>
#include <wayland-client.h>

#include <memory>

#include "args.hpp"
#include "src/waylandGfx.hpp"

int main(int argc, char **argv) {
  cpptrace::register_terminate_handler();

  Args::parse(argc, argv);

  Config::load();

  std::unique_ptr<PlatformGfx> gfx = std::make_unique<WaylandGfx>();

  gfx->init();

  auto loop_accounting_ticks = 0ull;
  auto loop_accounting_last = std::chrono::high_resolution_clock::now();

  gfx->platform_event_loop([&]() {
    auto const t_now = std::chrono::high_resolution_clock::now();

    auto const t_diff = std::chrono::duration_cast<std::chrono::milliseconds>(
                            t_now - loop_accounting_last)
                            .count();

    if (t_diff > 1000) {
      std::cerr << std::format("fps: {}\n", loop_accounting_ticks);
      loop_accounting_ticks = 0;
      loop_accounting_last = t_now;
    }

    loop_accounting_ticks++;

    return true;
  });

  return 0;
}