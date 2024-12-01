#pragma once

#include <limits>
static_assert(__cplusplus >= 202002L, "C++20 required");

#include <cpptrace.hpp>
#include <libdecor.h>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_wayland.h>
#include <wayland-client-protocol.h>
#include <wayland-client.h>

#include <atomic>
#include <format>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string_view>

#include "../args.hpp"
#include "platformGfx.hpp"
#include "vulkanCommon.hpp"
#include "xdg-decoration-client-protocol.h"
#include "xdg-shell-client-protocol.h"

/**
 *  Wayland-based concrete implementation on top of VulkanGfxBase
 */
struct WaylandGfx : public PlatformGfx, protected VulkanGfxBase {
  WaylandGfx() : VulkanGfxBase{this} {}
  virtual ~WaylandGfx() { VulkanGfxBase::~VulkanGfxBase(); }

  WaylandGfx(const WaylandGfx &) = delete;
  WaylandGfx(WaylandGfx &&) = delete;
  WaylandGfx &operator=(const WaylandGfx &) = delete;
  WaylandGfx &operator=(WaylandGfx &&) = delete;

  struct Window;

  struct Display {

    // dispatch of events will be done in the event loop from one of these
    // wl_display will be used if zxdg_decoration_manager_v1 is available
    wl_display *display = nullptr;
    libdecor *ld_context = nullptr;
    libdecor_frame *ld_frame = nullptr;

    Geometry geometry = {.width = 0, .height = 0};

    wl_registry *registry = nullptr;
    wl_compositor *compositor = nullptr;

    wl_output *output = nullptr;
    Geometry output_geometry = {.width = 0, .height = 0};

    wl_surface *surface = nullptr;

    // wl_shell *shell;
    xdg_wm_base *xdg_wm_base = nullptr;

    // decoration
    zxdg_decoration_manager_v1 *decoration_manager = nullptr;

    // input
    wl_seat *seat = nullptr;
    wl_keyboard *kbd = nullptr;
    wl_pointer *mouse = nullptr;
    bool has_kbd = false;
    bool has_pointer = false;

    ~Display() {
      if (ld_frame != nullptr) {
        libdecor_frame_close(ld_frame);
      }

      if (ld_context != nullptr) {
        libdecor_unref(ld_context);
      }

      if (surface != nullptr) {
        wl_surface_destroy(surface);
      }
    }

    Display(Display const &) = delete;
    Display(Display &&) = delete;
    Display &operator=(Display const &) = delete;
    Display &operator=(Display &&) = delete;

    Display() : display(wl_display_connect(nullptr)) {
      if (display == nullptr) {
        throw std::runtime_error(std::format("{}:{}:{}: Failed to connect to "
                                             "Wayland display server\n",
                                             __FILE__, __LINE__,
                                             __PRETTY_FUNCTION__));
      }

      registry = wl_display_get_registry(display);
      if (registry == nullptr) {
        throw std::runtime_error(std::format("{}:{}:{}: Failed to get "
                                             "Wayland registry\n",
                                             __FILE__, __LINE__,
                                             __PRETTY_FUNCTION__));
      }

      static wl_registry_listener registry_listener;
      registry_listener.global = [](void *data, wl_registry *registry,
                                    uint32_t name, const char *interface,
                                    uint32_t /*version*/) {
        auto *self = static_cast<Display *>(data);
        auto const siface = std::string_view(interface);

        if (siface == "wl_compositor") {
          self->compositor = static_cast<wl_compositor *>(
              wl_registry_bind(registry, name, &wl_compositor_interface,
                               wl_compositor_interface.version));
        } else if (siface == "wl_seat") {
          self->seat = static_cast<wl_seat *>(
              wl_registry_bind(registry, name, &wl_seat_interface, 8));
        } else if (siface == xdg_wm_base_interface.name) {
          self->xdg_wm_base = static_cast<struct xdg_wm_base *>(
              wl_registry_bind(registry, name, &xdg_wm_base_interface,
                               wl_output_interface.version));
          static auto listener = xdg_wm_base_listener{
              .ping = [](void * /*data*/, struct xdg_wm_base *xdg_wm_base,
                         uint32_t serial) {
                xdg_wm_base_pong(xdg_wm_base, serial);
              }};
          xdg_wm_base_add_listener(self->xdg_wm_base, &listener, self);
        } else if (siface == "wl_output") {
          self->output = static_cast<wl_output *>(
              wl_registry_bind(registry, name, &wl_output_interface,
                               wl_output_interface.version));
        } else if (siface == "zxdg_decoration_manager_v1") {
          self->decoration_manager =
              static_cast<zxdg_decoration_manager_v1 *>(wl_registry_bind(
                  registry, name, &zxdg_decoration_manager_v1_interface,
                  zxdg_decoration_manager_v1_interface.version));
        } else {
          if (Args::verbose() > 1) {
            std::cerr << std::format("{}:{} ignoring global {} ({})\n",
                                     __FILE__, __LINE__, name, interface);
          }
        }
      };

      registry_listener.global_remove =
          [](void * /*data*/, wl_registry * /*registry*/, uint32_t name) {
            // auto display = static_cast<Display *>(data);
            if (Args::verbose() > 0) {
              std::cerr << std::format(
                  "{}:{}:{}: removed global {} from registry\n", __FILE__,
                  __LINE__, __PRETTY_FUNCTION__, name);
            }
          };

      if (wl_registry_add_listener(registry, &registry_listener, this) != 0) {
        throw std::runtime_error(std::format("{}:{}:{}: Failed to add listener "
                                             "to Wayland registry\n",
                                             __FILE__, __LINE__,
                                             __PRETTY_FUNCTION__));
      }

      if (wl_display_roundtrip(display) == -1) {
        throw std::runtime_error(std::format("{}:{}:{}: Failed to roundtrip "
                                             "Wayland display\n",
                                             __FILE__, __LINE__,
                                             __PRETTY_FUNCTION__));
      }

      if (xdg_wm_base == nullptr || display == nullptr ||
          compositor == nullptr || output == nullptr) {
        throw std::runtime_error(std::format(
            "{}:{}:{}: Failed to bind/initialize base Wayland facilities\n",
            __FILE__, __LINE__, __PRETTY_FUNCTION__));
      }

      if (seat == nullptr) {
        throw std::runtime_error{
            std::format("{}:{}:{}: Wayland seat/input is required\n", __FILE__,
                        __LINE__, __PRETTY_FUNCTION__)};
      }

      static auto output_listener = wl_output_listener{};

      output_listener.geometry =
          [](void *data, wl_output * /*output*/, int32_t pos_x, int32_t pos_y,
             int32_t physical_width, int32_t physical_height, int32_t subpixel,
             const char *make, const char *model, int32_t transform) {
            auto *display = static_cast<Display *>(data);

            display->output_geometry = {
                .width = static_cast<uint32_t>(physical_width),
                .height = static_cast<uint32_t>(physical_height)};

            if (Args::verbose() > 0) {
              std::cerr << std::format(
                  "{}:{}: output geometry: x={}, y={}, "
                  "physical_width={}, "
                  "physical_height={}, subpixel={}, make={}, model={}, "
                  "transform={}\n",
                  __FILE__, __LINE__, pos_x, pos_y, physical_width,
                  physical_height, subpixel, make, model, transform);
            }
          };

      output_listener.mode = [](void *data, wl_output * /*output*/,
                                uint32_t flags, int32_t width, int32_t height,
                                int32_t refresh) {
        auto *display = static_cast<Display *>(data);
        display->geometry = {.width = static_cast<uint32_t>(width),
                             .height = static_cast<uint32_t>(height)};

        if (Args::verbose() > 0)
          std::cerr << std::format(
              "{}:{}: output mode: flags={}, width={}, height={}, "
              "refresh={}\n",
              __FILE__, __LINE__, flags, width, height, refresh);
      };

      output_listener.done = [](void * /*data*/, wl_output * /*output*/) {
        if (Args::verbose() > 0)
          std::cerr << std::format("{}:{}: output done\n", __FILE__, __LINE__);
      };

      output_listener.scale = [](void * /*data*/, wl_output * /*output*/,
                                 int32_t factor) {
        if (Args::verbose() > 0)
          std::cerr << std::format("{}:{} output scale: factor={}\n", __FILE__,
                                   __LINE__, factor);
      };

      output_listener.description = [](void * /*data*/, wl_output * /*output*/,
                                       const char *description) {
        if (Args::verbose() > 0)
          std::cerr << std::format("{}:{} output description: {}\n", __FILE__,
                                   __LINE__, description);
      };

      output_listener.name = [](void * /*data*/, wl_output * /*output*/,
                                const char *name) {
        if (Args::verbose() > 0)
          std::cerr << std::format("{}:{} output name: {}\n", __FILE__,
                                   __LINE__, name);
      };

      if (wl_output_add_listener(output, &output_listener, this) != 0) {
        throw std::runtime_error(std::format("{}:{}:{}: Failed to add "
                                             "listener to Wayland output\n",
                                             __FILE__, __LINE__,
                                             __PRETTY_FUNCTION__));
      }

      // init seat/input
      static const auto seat_listener = wl_seat_listener{
          .capabilities =
              [](void *data, wl_seat *seat, uint32_t capabilities) {
                auto *self = static_cast<Display *>(data);

                if (Args::verbose() > 0)
                  std::cerr << std::format("{}:{}: seat capabilities: {}\n",
                                           __FILE__, __LINE__, capabilities);

                if (capabilities & WL_SEAT_CAPABILITY_KEYBOARD) {
                  if (Args::verbose() > 0)
                    std::cerr << std::format("{}:{}: seat has keyboard\n",
                                             __FILE__, __LINE__);
                  self->kbd = wl_seat_get_keyboard(seat);
                  static auto const kbd_listener = wl_keyboard_listener{

                      .keymap =
                          [](void * /*data*/, wl_keyboard * /*kbd*/,
                             uint32_t format, int32_t keymap_fd,
                             uint32_t size) {
                            if (Args::verbose() > 0)
                              std::cerr << std::format(
                                  "{}:{}: keymap: format={}, fd={}, size = "
                                  "{}\n ",
                                  __FILE__, __LINE__, format, keymap_fd, size);
                          },

                      .enter =
                          [](void *data, wl_keyboard * /*kbd*/, uint32_t serial,
                             wl_surface *surface, wl_array * /*keys*/) {
                            auto *self = static_cast<Display *>(data);
                            if (surface == self->surface) {
                              self->has_kbd = true;
                            }

                            if (Args::verbose() > 1)
                              std::cerr << std::format(
                                  "{}:{}: key enter: serial={}\n", __FILE__,
                                  __LINE__, serial);
                          },

                      .leave =
                          [](void *data, wl_keyboard * /*kbd*/, uint32_t serial,
                             wl_surface *surface) {
                            auto *self = static_cast<Display *>(data);
                            if (surface == self->surface) {
                              self->has_kbd = false;
                            }
                            if (Args::verbose() > 1)
                              std::cerr << std::format(
                                  "{}:{}: key leave: serial={}\n", __FILE__,
                                  __LINE__, serial);
                          },
                      .key =
                          [](void * /*data*/, wl_keyboard * /*kbd*/,
                             uint32_t serial, uint32_t time, uint32_t key,
                             uint32_t state) {
                            if (Args::verbose() > 0)
                              std::cerr << std::format(
                                  "{}:{}: key event: serial={}, time={}, "
                                  "key={}, "
                                  "state={}\n",
                                  __FILE__, __LINE__, serial, time, key, state);
                          },

                      .modifiers =
                          [](void * /*data*/, wl_keyboard * /*kbd*/,
                             uint32_t serial, uint32_t mods_depressed,
                             uint32_t mods_latched, uint32_t mods_locked,
                             uint32_t group) {
                            if (Args::verbose() > 1)
                              std::cerr << std::format(
                                  "{}:{}: key modifiers: serial={}, "
                                  "mods_depressed={}, "
                                  "mods_latched={}, mods_locked={}, group = "
                                  "{}\n ",
                                  __FILE__, __LINE__, serial, mods_depressed,
                                  mods_latched, mods_locked, group);
                          },

                      .repeat_info =
                          [](void * /*data*/, wl_keyboard * /*kbd*/,
                             int32_t rate, int32_t delay) {
                            if (Args::verbose() > 1)
                              std::cerr << std::format(
                                  "{}:{}: key repeat info: rate={}, delay = "
                                  "{}\n ",
                                  __FILE__, __LINE__, rate, delay);
                          },

                  };

                  wl_keyboard_add_listener(self->kbd, &kbd_listener, self);
                }

                if (capabilities & WL_SEAT_CAPABILITY_POINTER) {
                  if (Args::verbose() > 0)
                    std::cerr << std::format("{}:{}: seat has pointer\n",
                                             __FILE__, __LINE__);

                  self->mouse = wl_seat_get_pointer(seat);

                  static auto const pointer_listener = wl_pointer_listener{
                      .enter =
                          [](void *data, wl_pointer * /*pointer*/,
                             uint32_t serial, wl_surface *surface,
                             wl_fixed_t surface_x, wl_fixed_t surface_y) {
                            auto *self = static_cast<Display *>(data);

                            if (surface == self->surface)
                              self->has_pointer = true;

                            auto const local_x = wl_fixed_to_int(surface_x);
                            auto const local_y = wl_fixed_to_int(surface_y);

                            if (Args::verbose() > 1)
                              std::cerr << std::format(
                                  "{}:{}: pointer enter: serial={}, x={}, "
                                  "y={}\n",
                                  __FILE__, __LINE__, serial, local_x, local_y);
                          },
                      .leave =
                          [](void *data, wl_pointer * /*pointer*/,
                             uint32_t serial, wl_surface *surface) {
                            auto *self = static_cast<Display *>(data);

                            if (self->surface == surface)
                              self->has_pointer = false;

                            if (Args::verbose() > 1)
                              std::cerr << std::format(
                                  "{}:{}: pointer leave: serial={}\n", __FILE__,
                                  __LINE__, serial);
                          },
                      .motion =
                          [](void *data, wl_pointer * /*pointer*/,
                             uint32_t time, wl_fixed_t surface_x,
                             wl_fixed_t surface_y) {
                            auto *self = static_cast<Display *>(data);

                            if (!self->has_pointer)
                              return;

                            if (Args::verbose() > 2) {
                              int frame_x = -1;
                              int frame_y = -1;

                              libdecor_frame_translate_coordinate(
                                  self->ld_frame, wl_fixed_to_int(surface_x),
                                  wl_fixed_to_int(surface_y), &frame_x,
                                  &frame_y);

                              std::cerr << "frame_x: " << frame_x
                                        << " frame_y: " << frame_y << '\n';
                            }

                            auto const local_x = wl_fixed_to_int(surface_x);
                            auto const local_y = wl_fixed_to_int(surface_y);

                            if (Args::verbose() > 2) {
                              std::cerr << std::format("{}:{}: pointer motion: "
                                                       "time={}, x={}, y={}\n",
                                                       __FILE__, __LINE__, time,
                                                       local_x, local_y);
                            }
                          },
                      .button =
                          [](void * /*data*/, wl_pointer * /*pointer*/,
                             uint32_t serial, uint32_t time, uint32_t button,
                             uint32_t state) {
                            if (Args::verbose() > 0)
                              std::cerr << std::format(
                                  "{}:{}: pointer button: serial={}, time={}, "
                                  "button = {}, state = {}\n ",
                                  __FILE__, __LINE__, serial, time, button,
                                  state);
                          },
                      .axis =
                          [](void * /*data*/, wl_pointer * /*pointer*/,
                             uint32_t time, uint32_t axis, wl_fixed_t value) {
                            if (Args::verbose() > 0)
                              std::cerr << std::format(
                                  "{}:{}: pointer axis: time={}, axis={}, "
                                  "value={}\n",
                                  __FILE__, __LINE__, time, axis, value);
                          },
                      .frame =
                          [](void * /*data*/, wl_pointer * /*pointer*/) {
                            if (Args::verbose() > 2)
                              std::cerr << std::format("{}:{}: pointer frame\n",
                                                       __FILE__, __LINE__);
                          },
                      .axis_source =
                          [](void * /*data*/, wl_pointer * /*pointer*/,
                             uint32_t axis_source) {
                            if (Args::verbose() > 0)
                              std::cerr << std::format(
                                  "{}:{}: pointer axis source: "
                                  "axis_source={}\n",
                                  __FILE__, __LINE__, axis_source);
                          },
                      .axis_stop =
                          [](void * /*data*/, wl_pointer * /*pointer*/,
                             uint32_t time, uint32_t axis) {
                            if (Args::verbose() > 0)
                              std::cerr << std::format(
                                  "{}:{}: pointer axis stop: time={}, "
                                  "axis={}\n",
                                  __FILE__, __LINE__, time, axis);
                          },
                      .axis_discrete =
                          [](void * /*data*/, wl_pointer * /*pointer*/,
                             uint32_t axis, int32_t discrete) {
                            if (Args::verbose() > 0)
                              std::cerr << std::format(
                                  "{}:{}: pointer axis discrete: axis={}, "
                                  "discrete={}\n",
                                  __FILE__, __LINE__, axis, discrete);
                          },

                      .axis_value120 =
                          [](void * /*data*/, wl_pointer * /*pointer*/,
                             uint32_t axis, int32_t value) {
                            if (Args::verbose() > 0)
                              std::cerr << std::format(
                                  "{}:{}: pointer axis value120: axis={}, "
                                  "value={}\n",
                                  __FILE__, __LINE__, axis, value);
                          },

                      .axis_relative_direction =
                          [](void * /*data*/, wl_pointer * /*pointer*/,
                             uint32_t axis, uint32_t direction) {
                            if (Args::verbose() > 0)
                              std::cerr << std::format(
                                  "{}:{}: pointer axis relative direction: "
                                  "axis={}, direction={}\n",
                                  __FILE__, __LINE__, axis, direction);
                          },
                  };

                  wl_pointer_add_listener(self->mouse, &pointer_listener, self);
                }
              },
          .name =
              [](void * /*data*/, wl_seat * /*seat*/, const char *name) {
                if (Args::verbose() > 0)
                  std::cerr << std::format("{}:{}: seat name: {}\n", __FILE__,
                                           __LINE__, name);
              }};

      if (wl_seat_add_listener(seat, &seat_listener, this) != 0) {
        throw std::runtime_error(std::format("{}:{}:{}: Failed to add "
                                             "listener to Wayland seat\n",
                                             __FILE__, __LINE__,
                                             __PRETTY_FUNCTION__));
      }

      surface = wl_compositor_create_surface(compositor);
      wl_display_roundtrip(display);

      std::cerr << "Wayland display initialized\n";
    }

    [[nodiscard]] auto dispatchEvents() const {
      auto retv = wl_display_dispatch(display);
      if (ld_context != nullptr) {
        return libdecor_dispatch(ld_context, -1) < 0 ? -1 : retv;
      }
      return retv;
    }
  };

  struct Window {
    std::shared_ptr<Display> display;
    xdg_surface *xdg_surface = nullptr;
    xdg_toplevel *xdg_toplevel = nullptr;
    Geometry geometry{};

    std::atomic<bool> configured = false;
    std::atomic<bool> closed = false;

    std::function<void()> redraw_fn;
    std::function<void(Geometry)> on_resize;

    ~Window() {
      if (Args::verbose() > 0)
        std::cerr << std::format("{}:{}:{}: destroying window\n", __FILE__,
                                 __LINE__, __PRETTY_FUNCTION__);
      if (xdg_toplevel != nullptr)
        xdg_toplevel_destroy(xdg_toplevel);
      if (xdg_surface != nullptr)
        xdg_surface_destroy(xdg_surface);
    }

    Window() = delete;
    Window(Window const &) = delete;
    Window(Window &&) = delete;
    Window &operator=(Window const &) = delete;
    Window &operator=(Window &&) = delete;

    Window(const std::shared_ptr<Display> &display,
           std::function<void()> &&on_redraw,
           std::function<void(Geometry)> &&on_resize)
        : display(display), redraw_fn(std::move(on_redraw)),
          on_resize(std::move(on_resize)) {
      geometry.width = std::get<int64_t>(Config::get(Config::Key::GFX_WIDTH));
      geometry.height = std::get<int64_t>(Config::get(Config::Key::GFX_HEIGHT));

      if (display->decoration_manager != nullptr) {
        xdg_surface =
            xdg_wm_base_get_xdg_surface(display->xdg_wm_base, display->surface);

        static auto surface_listener = xdg_surface_listener{
            .configure = [](void *data, struct xdg_surface *xdg_surface,
                            uint32_t serial) {
              auto *window = static_cast<Window *>(data);
              xdg_surface_ack_configure(xdg_surface, serial);

              if (window->display->decoration_manager) {
                window->xdg_toplevel =
                    xdg_surface_get_toplevel(window->xdg_surface);
                struct zxdg_toplevel_decoration_v1 *decoration =
                    zxdg_decoration_manager_v1_get_toplevel_decoration(
                        window->display->decoration_manager,
                        window->xdg_toplevel);

                // Request server-side decorations
                zxdg_toplevel_decoration_v1_set_mode(
                    decoration, ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
              } else if (Args::verbose() > 0) {
              }

              window->configured.store(true);
              window->configured.notify_all();
            }};

        xdg_surface_add_listener(xdg_surface, &surface_listener, this);

        static auto toplevel_listener = xdg_toplevel_listener{
            .configure =
                [](void * /*data*/, struct xdg_toplevel * /*xdg_toplevel*/,
                   int32_t width, int32_t height,
                   struct wl_array * /*states*/) {
                  // auto *window = static_cast<Window *>(data);
                  // window->geometry = {static_cast<uint32_t>(width),
                  //                     static_cast<uint32_t>(height)};
                  if (Args::verbose() > 0)
                    std::cerr << std::format(
                        "{}:{}: toplevel configure: width={}, height={}\n",
                        __FILE__, __LINE__, width, height);
                },
            .close =
                [](void *data, struct xdg_toplevel * /*xdg_toplevel*/) {
                  auto *window = static_cast<Window *>(data);

                  window->closed.store(true);

                  if (Args::verbose() > 0)
                    std::cerr << std::format("{}:{}: toplevel close\n",
                                             __FILE__, __LINE__);
                },
            .configure_bounds =
                [](void * /*data*/, struct xdg_toplevel * /*xdg_toplevel*/,
                   int32_t width, int32_t height) {
                  if (Args::verbose() > 0)
                    std::cerr
                        << std::format("{}:{}: toplevel configure_bounds: "
                                       "width={}, height={}\n",
                                       __FILE__, __LINE__, width, height);
                },
            .wm_capabilities =
                [](void * /*data*/, struct xdg_toplevel * /*xdg_toplevel*/,
                   wl_array * /*capabilities*/) {
                  if (Args::verbose() > 0)
                    std::cerr
                        << std::format("{}:{}: toplevel wm_capabilities\n",
                                       __FILE__, __LINE__);
                }};

        xdg_toplevel_add_listener(xdg_toplevel, &toplevel_listener, this);

        xdg_toplevel_set_title(xdg_toplevel, "HotAir");
      } else {
        std::cerr << std::format("{}:{}: no XDG decoration manager; "
                                 "attempt CSD with libdecor\n",
                                 __FILE__, __LINE__);

        // libdecor
        static struct libdecor_interface ld_iface = {
            .error =
                [](libdecor * /*ctx*/, libdecor_error /*error*/,
                   const char *message) {
                  std::cerr << std::format("{}:{}: libdecor error: {}\n",
                                           __FILE__, __LINE__, message);
                },
        };

        display->ld_context = libdecor_new(display->display, &ld_iface);

        if (display->ld_context == nullptr) {
          throw std::runtime_error{
              std::format("{}:{}: libdecor_new failed\n", __FILE__, __LINE__)};
        }

        static libdecor_frame_interface ld_frame_iface = {
            .configure =
                [](libdecor_frame *frame, libdecor_configuration *configuration,
                   void *data) {
                  auto *window = static_cast<Window *>(data);

                  int width = 0;
                  int height = 0;

                  const bool is_initial_frame =
                      !libdecor_configuration_get_content_size(
                          configuration, frame, &width, &height);

                  width = (width == 0) ? static_cast<int>(std::clamp<uint32_t>(
                                             window->geometry.width, 1U,
                                             std::numeric_limits<int>::max()))
                                       : width;
                  height = (height == 0)
                               ? static_cast<int>(std::clamp<uint32_t>(
                                     window->geometry.height, 1U,
                                     std::numeric_limits<int>::max()))
                               : height;

                  if (Args::verbose() > 0)
                    std::cerr << std::format(
                        "{}:{}: libdecor configure: width={}, height={}\n",
                        __FILE__, __LINE__, width, height);

                  libdecor_state *state = libdecor_state_new(width, height);
                  libdecor_frame_commit(frame, state, configuration);
                  libdecor_state_free(state);

                  window->configured.store(true);
                  window->configured.notify_all();

                  if (!is_initial_frame) {
                    auto const new_geometry =
                        Geometry{.width = static_cast<uint32_t>(width),
                                 .height = static_cast<uint32_t>(height)};

                    window->on_resize(new_geometry);

                    window->redraw_fn();
                  }

                  wl_surface_commit(window->display->surface);
                },
            .close =
                [](libdecor_frame * /*frame*/, void *data) {
                  auto *window = static_cast<Window *>(data);
                  window->closed.store(true);
                  window->closed.notify_all();
                },

            .commit =
                [](libdecor_frame * /*frame*/, void *data) {
                  auto *window = static_cast<Window *>(data);
                  wl_surface_commit(window->display->surface);
                },

            .dismiss_popup = [](libdecor_frame *frame, const char *seat_name,
                                void *data) {},
        };

        display->ld_frame = libdecor_decorate(
            display->ld_context, display->surface, &ld_frame_iface, this);
        libdecor_frame_set_app_id(display->ld_frame, "HotAir");
        libdecor_frame_set_title(display->ld_frame, "HotAir");
        libdecor_frame_map(display->ld_frame);
      }

      wl_surface_commit(display->surface);

      while (!configured) {
        wl_display_dispatch(display->display);
      }

      std::cerr << std::format("{}:{}: Wayland window initialized: {}x{}\n",
                               __FILE__, __LINE__, geometry.width,
                               geometry.height);
    }
  };

  std::shared_ptr<Display> display;
  std::shared_ptr<Window> window;
  vk::SurfaceKHR surface;

  std::atomic<bool> initialized = false;
  std::function<bool()> on_tick = []() { return true; };

  auto getGeometry() -> Geometry override { return window->geometry; }

  void platformEventLoop(std::function<bool()> &&on_tick) override {

    assert(window->display->surface);

    if (on_tick) {
      this->on_tick = std::move(on_tick);
    }

    {

      static const wl_callback_listener frame_listener = wl_callback_listener{
          .done = [](void *data, wl_callback *frame_cback, uint32_t /*time*/) {
            auto *self = static_cast<WaylandGfx *>(data);
            wl_callback_destroy(frame_cback);

            if (Args::verbose() > 2)
              std::cerr << ".";

            if (self->window->closed) {
              if (Args::verbose() > 0)
                std::cerr << std::format(
                    "{}:{}: redrawing halted: window closed\n", __FILE__,
                    __LINE__);
              return;
            }

            if (!self->on_tick()) {
              if (Args::verbose() > 0)
                std::cerr << std::format(
                    "{}:{}: redrawing halted by main driver\n", __FILE__,
                    __LINE__);
              return;
            }

            self->redraw();

            auto *next_cb = wl_surface_frame(self->window->display->surface);
            wl_callback_add_listener(next_cb, &frame_listener, self);

            wl_surface_commit(self->window->display->surface);
            //  wl_display_flush(self->display->display);
          }};

      auto *cback = wl_surface_frame(window->display->surface);
      wl_callback_add_listener(cback, &frame_listener, this);
    }

    while (display->dispatchEvents() != -1) {
      if (window->closed)
        break;
    }
  }

  void init() override {
    display = std::make_shared<Display>();

    window = std::make_shared<Window>(
        display,
        [this]() { // on_redraw
          this->redraw();
        },
        [this](Geometry new_geometry) { // on_resize
          if (new_geometry == window->geometry) {
            if (Args::verbose() > 1)
              std::cerr << std::format("{}:{}: geometry unchanged\n", __FILE__,
                                       __LINE__);
            return;
          }

          window->geometry = new_geometry;

          Config::set(Config::Key::GFX_WIDTH, new_geometry.width);
          Config::set(Config::Key::GFX_HEIGHT, new_geometry.height);

          if (Args::verbose() > 0)
            std::cerr << std::format(
                "{}:{}: re-initializing Vulkan on resize\n", __FILE__,
                __LINE__);

          this->initialized.store(false);
          this->initialized.notify_all();

          VulkanGfxBase::destroy();
          VulkanGfxBase::init(nullptr);

          this->initialized.store(true);
          this->initialized.notify_all();
        });

    /* VulkanGfxBase::*/ VulkanGfxBase::init([&]() {
      auto const create_info = VkWaylandSurfaceCreateInfoKHR{
          .sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR,
          .pNext = nullptr,
          .flags = 0,
          .display = display->display,
          .surface = display->surface,
      };

      VkSurfaceKHR vksurface = nullptr;

      if (auto result = vkCreateWaylandSurfaceKHR(
              /* VulkanGfxBase:: */ instance, &create_info, nullptr,
              &vksurface);
          result != VK_SUCCESS || vksurface == nullptr) {
        throw std::runtime_error(
            std::format("{}:{}:{}: Failed to create Wayland "
                        "surface\n",
                        __FILE__, __LINE__, __PRETTY_FUNCTION__));
      }

      surface = vk::SurfaceKHR(vksurface);

      if (Args::verbose() > 0)
        std::cerr << "Wayland surface created\n";

      return &surface;
    });

    initialized.store(true);
    initialized.notify_all();

    redraw();

    std::cerr << "WaylandGfx initialized\n";
  }

  void redraw() {
    if (initialized) {
      if (device.waitForFences(1, &inFlightFence, vk::Bool32{true},
                               UINT64_MAX) != vk::Result::eSuccess) {
        throw std::runtime_error{std::format("{}:{}: vkWaitForFences erred out",
                                             __FILE__, __LINE__)};
      }

      if (device.resetFences(1, &inFlightFence) != vk::Result::eSuccess) {
        throw std::runtime_error{
            std::format("{}:{}: vkResetFences erred out", __FILE__, __LINE__)};
      }

      uint32_t current_image_index = 0;

      if (device.acquireNextImageKHR(
              swapchain, UINT64_MAX, imageAvailableSemaphore, nullptr,
              &current_image_index) != vk::Result::eSuccess) {
        throw std::runtime_error{std::format(
            "{}:{}: vkAcquireNextImageKHR erred out", __FILE__, __LINE__)};
      }

      commandBuffers.graphics[current_image_index].reset();
      commandBuffers.present[0].reset();

      vk::CommandBufferBeginInfo begin_info;
      commandBuffers.graphics[current_image_index].begin(begin_info);

      vk::RenderPassBeginInfo render_pass_info;
      render_pass_info.renderPass = renderPass;
      render_pass_info.framebuffer = framebuffers[current_image_index];
      render_pass_info.renderArea.offset = {{0, 0}};
      render_pass_info.renderArea.extent = extent;

      vk::ClearValue clear_color =
          vk::ClearColorValue(std::array{1.0f, 0.3f, 0.0f, 1.0f});
      render_pass_info.clearValueCount = 1;
      render_pass_info.pClearValues = &clear_color;

      commandBuffers.graphics[current_image_index].beginRenderPass(
          render_pass_info, vk::SubpassContents::eInline);

      // commandBuffers.graphics[currentImageIndex].draw(0, 0, 0, 0);

      commandBuffers.graphics[current_image_index].endRenderPass();

      commandBuffers.graphics[current_image_index].end();

      auto submit_info = vk::SubmitInfo();
      auto wait_stages = std::array<vk::PipelineStageFlags, 1>{
          vk::PipelineStageFlagBits::eColorAttachmentOutput};
      submit_info.waitSemaphoreCount = 1;
      submit_info.pWaitSemaphores = &imageAvailableSemaphore;

      submit_info.pWaitDstStageMask = wait_stages.data();
      submit_info.commandBufferCount = 1;

      submit_info.pCommandBuffers =
          &commandBuffers.graphics[current_image_index];
      submit_info.pSignalSemaphores = &renderFinishedSemaphore;
      submit_info.signalSemaphoreCount = 1;

      // queue.submit(uint32_t submitCount, const vk::SubmitInfo *pSubmits,
      // vk::Fence fence); queue.submit(const vk::ArrayProxy<const
      // vk::SubmitInfo> &submits)

      if (queue.submit(1, &submit_info, inFlightFence) !=
          vk::Result::eSuccess) {
        throw std::runtime_error{
            std::format("{}:{}: vkQueue.submit erred out", __FILE__, __LINE__)};
      }

      auto present_info = vk::PresentInfoKHR{};
      present_info.waitSemaphoreCount = 1;
      present_info.pWaitSemaphores = &renderFinishedSemaphore;
      present_info.swapchainCount = 1;
      present_info.pSwapchains = &swapchain;
      present_info.pImageIndices = &current_image_index;

      if (presentQueue.presentKHR(present_info) != vk::Result::eSuccess) {
        throw std::runtime_error{std::format(
            "{}:{}: vkQueue.presentKHR erred out", __FILE__, __LINE__)};
      }
    }
  };
};
