#pragma once

#include <etl/to_string.h>

namespace troll {
  // https://gist.github.com/cleoold/0f992b0fdee7dc3d6d0ed4497044d261 (young)
  char *sformat_impl(char *dest, const char *format) {
    while (*format) {
      *dest++ = *format++;
    }
    return dest;
  }

  template<class Arg0, class ...Args>
  char *sformat_impl(char *dest, const char *format, const Arg0 &a0, const Args &...args) {
    while (*format) {
      if (*format == '{' && *(format + 1) == '}') {
        etl::string_ext s{dest, static_cast<size_t>(-1)};
        etl::to_string(a0, s);
        return sformat_impl(dest + s.length(), format + 2, args...);
      }
      *dest++ = *format++;
    }
    __builtin_unreachable();
  }

  /**
   * formats string into buffer. returns length excluding \0.
  */
  template<class ...Args>
  size_t sformat(char *dest, const char *format, const Args &...args) {
    return sformat_impl(dest, format, args...) - dest;
  }
}  // namespace troll
