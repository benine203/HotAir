#pragma once
#include <cstdint>
#include <functional>

/**
 * Abstract over platform-specific graphics code.
 *
 * This class is meant to be subclassed by platform-specific implementations
 * (e.g. WaylandGfx). It factors out common traits:
 *      - Geometry of display/window and accessor
 *      - Prescribes an init() method
 *      - event loop with user-supplied callbacks
 */
struct PlatformGfx {
  virtual ~PlatformGfx() = default;

  struct Geometry {
    uint32_t width;
    uint32_t height;
    bool operator==(Geometry const &other) const {
      return width == other.width && height == other.height;
    }
  };

  virtual Geometry getGeometry() = 0;

  virtual void init() = 0;

  /**
   * Event loop with user-supplied callback.
   * Halts when on_tick returns false.
   */
  virtual void platformEventLoop(std::function<bool()> &&on_tick) = 0;
};
