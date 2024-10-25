#pragma once
#include <cstdint>

struct PlatformGfx {
  struct Geometry {
    uint32_t width;
    uint32_t height;
  };

  virtual Geometry getGeometry() = 0;

  virtual void init() = 0;
};
