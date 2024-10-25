#pragma once

static_assert(__cplusplus >= 202002L, "C++20 required");

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_wayland.h>
#include <wayland-client-protocol.h>
#include <wayland-client.h>

#include <format>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include "../args.hpp"
#include "platformGfx.hpp"
#include "vulkanCommon.hpp"
#include "xdg-shell-client-protocol.h"

struct WaylandGfx : public PlatformGfx, public VulkanGfxBase<WaylandGfx> {
  WaylandGfx() : VulkanGfxBase<WaylandGfx>(this) {}

  struct Window;

  struct Display {
    wl_display *display;
    Geometry geometry;

    wl_registry *registry;
    wl_registry_listener registry_listener;
    wl_compositor *compositor;

    wl_output *output;
    Geometry output_geometry;

    // wl_shell *shell;
    xdg_wm_base *xdg_wm_base;

    Display() {
      display = wl_display_connect(nullptr);
      if (!display) {
        throw std::runtime_error(std::format("{}:{}:{}: Failed to connect to "
                                             "Wayland display server\n",
                                             __FILE__, __LINE__,
                                             __PRETTY_FUNCTION__));
      }

      registry = wl_display_get_registry(display);
      if (!registry) {
        throw std::runtime_error(std::format("{}:{}:{}: Failed to get "
                                             "Wayland registry\n",
                                             __FILE__, __LINE__,
                                             __PRETTY_FUNCTION__));
      }

      registry_listener.global = [](void *data, wl_registry *registry,
                                    uint32_t name, const char *interface,
                                    uint32_t version) {
        auto display = static_cast<Display *>(data);
        auto const siface = std::string_view(interface);

        if (siface == "wl_compositor") {
          display->compositor = static_cast<wl_compositor *>(
              wl_registry_bind(registry, name, &wl_compositor_interface,
                               wl_compositor_interface.version));
        } else if (siface == xdg_wm_base_interface.name) {
          display->xdg_wm_base = static_cast<struct xdg_wm_base *>(
              wl_registry_bind(registry, name, &xdg_wm_base_interface,
                               wl_output_interface.version));
          static auto listener = xdg_wm_base_listener{
              .ping = [](void *data, struct xdg_wm_base *xdg_wm_base,
                         uint32_t serial) {
                xdg_wm_base_pong(xdg_wm_base, serial);
              }};
          xdg_wm_base_add_listener(display->xdg_wm_base, &listener, display);
        } else if (siface == "wl_output") {
          display->output = static_cast<wl_output *>(
              wl_registry_bind(registry, name, &wl_output_interface,
                               wl_output_interface.version));
        } else {
          if (Args::verbose() > 1)
            std::cerr << std::format("{}:{} ignoring global {} ({})\n",
                                     __FILE__, __LINE__, name, interface);
        }
      };

      registry_listener.global_remove = [](void *data, wl_registry *registry,
                                           uint32_t name) {
        // auto display = static_cast<Display *>(data);
        if (Args::verbose() > 0)
          std::cerr << std::format(
              "{}:{}:{}: removed global {} from registry\n", __FILE__, __LINE__,
              __PRETTY_FUNCTION__, name);
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

      if (!*this) {
        throw std::runtime_error(std::format("{}:{}:{}: Failed to bind to "
                                             "Wayland compositor and shell\n",
                                             __FILE__, __LINE__,
                                             __PRETTY_FUNCTION__));
      }

      static auto output_listener = wl_output_listener{};

      output_listener.geometry =
          [](void *data, wl_output *output, int32_t x, int32_t y,
             int32_t physical_width, int32_t physical_height, int32_t subpixel,
             const char *make, const char *model, int32_t transform) {
            auto display = static_cast<Display *>(data);

            display->output_geometry = {static_cast<uint32_t>(physical_width),
                                        static_cast<uint32_t>(physical_height)};

            if (Args::verbose() > 0)
              std::cerr << std::format(
                  "{}:{}: output geometry: x={}, y={}, "
                  "physical_width={}, "
                  "physical_height={}, subpixel={}, make={}, model={}, "
                  "transform={}\n",
                  __FILE__, __LINE__, x, y, physical_width, physical_height,
                  subpixel, make, model, transform);
          };

      output_listener.mode = [](void *data, wl_output *output, uint32_t flags,
                                int32_t width, int32_t height,
                                int32_t refresh) {
        auto *display = static_cast<Display *>(data);
        display->geometry = {static_cast<uint32_t>(width),
                             static_cast<uint32_t>(height)};

        if (Args::verbose() > 0)
          std::cerr << std::format(
              "{}:{}: output mode: flags={}, width={}, height={}, "
              "refresh={}\n",
              __FILE__, __LINE__, flags, width, height, refresh);
      };

      output_listener.done = [](void *data, wl_output *output) {
        if (Args::verbose() > 0)
          std::cerr << std::format("{}:{}: output done\n", __FILE__, __LINE__);
      };

      output_listener.scale = [](void *data, wl_output *output,
                                 int32_t factor) {
        if (Args::verbose() > 0)
          std::cerr << std::format("{}:{} output scale: factor={}\n", __FILE__,
                                   __LINE__, factor);
      };

      output_listener.description = [](void *data, wl_output *output,
                                       const char *description) {
        if (Args::verbose() > 0)
          std::cerr << std::format("{}:{} output description: {}\n", __FILE__,
                                   __LINE__, description);
      };

      output_listener.name = [](void *data, wl_output *output,
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

      wl_display_roundtrip(display);

      std::cerr << "Wayland display initialized\n";
    }

    operator bool() const {
      return display && compositor && output && xdg_wm_base;
    }

    operator wl_display *() const { return display; }
    operator wl_registry *() const { return registry; }
    operator wl_compositor *() const { return compositor; }
    operator wl_output *() const { return output; }

    Display &operator=(wl_display *display) {
      this->display = display;
      return *this;
    }
  };

  struct Window {
    std::shared_ptr<Display> display;
    wl_surface *surface = nullptr;
    xdg_surface *xdg_surface = nullptr;
    xdg_toplevel *xdg_toplevel = nullptr;
    Geometry geometry;

    bool configured = false;
    bool closed = false;

    ~Window() {
      if (Args::verbose() > 0)
        std::cerr << std::format("{}:{}:{}: destroying window\n", __FILE__,
                                 __LINE__, __PRETTY_FUNCTION__);
      if (xdg_toplevel)
        xdg_toplevel_destroy(xdg_toplevel);
      if (xdg_surface)
        xdg_surface_destroy(xdg_surface);
      if (surface)
        wl_surface_destroy(surface);
    }

    Window(std::shared_ptr<Display> display) : display(display) {
      geometry = display->geometry;
      geometry.width /= 2;
      geometry.height /= 2;

      surface = wl_compositor_create_surface(display->compositor);
      xdg_surface = xdg_wm_base_get_xdg_surface(display->xdg_wm_base, surface);
      xdg_toplevel = xdg_surface_get_toplevel(xdg_surface);

      static auto surface_listener = xdg_surface_listener{
          .configure = [](void *data, struct xdg_surface *xdg_surface,
                          uint32_t serial) {
            auto *window = static_cast<Window *>(data);
            xdg_surface_ack_configure(xdg_surface, serial);
            window->configured = true;
          }};

      xdg_surface_add_listener(xdg_surface, &surface_listener, this);

      static auto toplevel_listener = xdg_toplevel_listener{
          .configure =
              [](void *data, struct xdg_toplevel *xdg_toplevel, int32_t width,
                 int32_t height, struct wl_array *states) {
                auto window = static_cast<Window *>(data);
                // window->geometry = {static_cast<uint32_t>(width),
                //                     static_cast<uint32_t>(height)};
                if (Args::verbose() > 0)
                  std::cerr << std::format(
                      "{}:{}: toplevel configure: width={}, height={}\n",
                      __FILE__, __LINE__, width, height);
              },
          .close =
              [](void *data, struct xdg_toplevel *xdg_toplevel) {
                auto window = static_cast<Window *>(data);
                window->closed = true;
                if (Args::verbose() > 0)
                  std::cerr << std::format("{}:{}: toplevel close\n", __FILE__,
                                           __LINE__);
              },
          .configure_bounds =
              [](void *data, struct xdg_toplevel *xdg_toplevel, int32_t width,
                 int32_t height) {
                if (Args::verbose() > 0)
                  std::cerr << std::format("{}:{}: toplevel configure_bounds: "
                                           "width={}, height={}\n",
                                           __FILE__, __LINE__, width, height);
              },
          .wm_capabilities =
              [](void *data, struct xdg_toplevel *xdg_toplevel,
                 wl_array *capabilities) {
                if (Args::verbose() > 0)
                  std::cerr << std::format("{}:{}: toplevel wm_capabilities\n",
                                           __FILE__, __LINE__);
              }};

      xdg_toplevel_add_listener(xdg_toplevel, &toplevel_listener, this);

      xdg_toplevel_set_title(xdg_toplevel, "Wayland Window");

      wl_surface_commit(surface);

      while (!configured) {
        wl_display_dispatch(*display);
      }

      std::cerr << std::format("{}:{}: Wayland window initialized: {}x{}\n",
                               __FILE__, __LINE__, geometry.width,
                               geometry.height);
    }
  };

  std::shared_ptr<Display> display;
  std::shared_ptr<Window> window;

  vk::SurfaceKHR surface;

  auto getGeometry() -> Geometry override { return window->geometry; }

  void init() override {
    display = std::make_shared<Display>();

    if (!*display) {
      throw std::runtime_error(std::format("{}:{}:{}: Failed to initialize "
                                           "Wayland display\n",
                                           __FILE__, __LINE__,
                                           __PRETTY_FUNCTION__));
    }

    window = std::make_shared<Window>(display);

    VulkanGfxBase::init([&]() {
      auto const create_info = VkWaylandSurfaceCreateInfoKHR{
          .sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR,
          .pNext = nullptr,
          .flags = 0,
          .display = display->display,
          .surface = window->surface,
      };

      VkSurfaceKHR vksurface;

      if (auto result = vkCreateWaylandSurfaceKHR(
              VulkanGfxBase::instance, &create_info, nullptr, &vksurface);
          result != VK_SUCCESS) {
        throw std::runtime_error(
            std::format("{}:{}:{}: Failed to create Wayland "
                        "surface\n",
                        __FILE__, __LINE__, __PRETTY_FUNCTION__));
      }

      surface = vk::SurfaceKHR(vksurface);

      std::cerr << "Wayland surface created\n";
      return &surface;
    });

    device.waitForFences(1, &inFlightFence, true, UINT64_MAX);
    device.resetFences(1, &inFlightFence);

    uint32_t currentImageIndex;
    device.acquireNextImageKHR(swapchain, UINT64_MAX, imageAvailableSemaphore,
                               nullptr, &currentImageIndex);

    commandBuffers.graphics[currentImageIndex].reset();
    commandBuffers.present[currentImageIndex].reset();
    // commandBuffers.transfer[currentImageIndex].reset();
    // commandBuffers.compute[currentImageIndex].reset();

    vk::CommandBufferBeginInfo beginInfo;
    commandBuffers.graphics[currentImageIndex].begin(beginInfo);

    vk::RenderPassBeginInfo renderPassInfo;
    renderPassInfo.renderPass = renderPass;
    renderPassInfo.framebuffer = framebuffers[currentImageIndex];
    renderPassInfo.renderArea.offset = {{0, 0}};
    renderPassInfo.renderArea.extent = extent;

    vk::ClearValue clearColor =
        vk::ClearColorValue(std::array{1.0f, 0.3f, 0.0f, 1.0f});
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;

    commandBuffers.graphics[currentImageIndex].beginRenderPass(
        renderPassInfo, vk::SubpassContents::eInline);

    // commandBuffers.graphics[currentImageIndex].draw(0, 0, 0, 0);

    commandBuffers.graphics[currentImageIndex].endRenderPass();

    commandBuffers.graphics[currentImageIndex].end();

    auto submitInfo = vk::SubmitInfo();
    vk::PipelineStageFlags waitStages[] = {
        vk::PipelineStageFlagBits::eColorAttachmentOutput};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &imageAvailableSemaphore;

    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;

    submitInfo.pCommandBuffers = &commandBuffers.graphics[currentImageIndex];
    submitInfo.pSignalSemaphores = &renderFinishedSemaphore;
    submitInfo.signalSemaphoreCount = 1;

    // queue.submit(uint32_t submitCount, const vk::SubmitInfo *pSubmits,
    // vk::Fence fence); queue.submit(const vk::ArrayProxy<const vk::SubmitInfo>
    // &submits)

    if (queue.submit(1, &submitInfo, inFlightFence) != vk::Result::eSuccess) {
      throw std::runtime_error{
          std::format("{}:{}: vkQueue.submit erred out", __FILE__, __LINE__)};
    }

    auto presentInfo = vk::PresentInfoKHR{};
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &renderFinishedSemaphore;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchain;
    presentInfo.pImageIndices = &currentImageIndex;

    if (presentQueue.presentKHR(presentInfo) != vk::Result::eSuccess) {
      throw std::runtime_error{std::format(
          "{}:{}: vkQueue.presentKHR erred out", __FILE__, __LINE__)};
    }

    std::cerr << "WaylandGfx initialized\n";
  }
};
