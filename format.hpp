#pragma once

#include <etl/to_string.h>

namespace troll {
  // https://gist.github.com/cleoold/0f992b0fdee7dc3d6d0ed4497044d261 (young)
  char *snformat_impl(char *dest, size_t destlen, const char *format) {
    for (size_t i = destlen - 1; *format && i--;) {
      *dest++ = *format++;
    }
    *dest = '\0';
    return dest;
  }

  template<class Arg0, class ...Args>
  char *snformat_impl(char *dest, size_t destlen, const char *format, const Arg0 &a0, const Args &...args) {
    for (size_t i = destlen - 1; *format && i;) {
      if (*format == '{' && *(format + 1) == '}') {
        size_t real_len = i + 1;
        ::etl::string_ext s{dest, real_len};
        ::etl::to_string(a0, s);
        char *end = dest + s.length();
        return s.length() < real_len ? snformat_impl(end, real_len - s.length(), format + 2, args...) : end;
      }
      *dest++ = *format++;
      --i;
    }
    *dest = '\0';
    return dest;
  }

  /**
   * formats string into the buffer. returns length of result string excluding \0.
   * the function does not overflow the buffer.
   * the dest buffer needs an extra char to hold \0.
  */
  template<class ...Args>
  size_t snformat(char *dest, size_t destlen, const char *format, const Args &...args) {
    return snformat_impl(dest, destlen, format, args...) - dest;
  }

  /**
   * similar to the other snformat but the buffer size is automatically deduced.
  */
  template<size_t N, class ...Args>
  size_t snformat(char (&dest)[N], const char *format, const Args &...args) {
    static_assert(N);
    return snformat(dest, N, format, args...);
  }
}  // namespace troll
