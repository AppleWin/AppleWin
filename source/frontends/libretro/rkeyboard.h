#pragma once

#include <cstddef>
#include <cstdint>

namespace ra2
{
  enum class KeyboardType { ASCII, Original };
  bool getApple2Character(size_t keycode, bool shift, bool ctrl, uint8_t & out);
}
