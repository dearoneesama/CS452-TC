#pragma once

#include <etl/to_arithmetic.h>
#include "format.hpp"

namespace troll {

  constexpr bool is_digit(char c) noexcept {
    return c >= '0' && c <= '9';
  }

  constexpr bool is_white_space(char c) noexcept {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
  }

  constexpr bool is_non_white_space(char c) noexcept {
    return !is_white_space(c);
  }

  template<class F>
  constexpr size_t eat_while(const char *test, size_t test_len, F &&f) noexcept {
    size_t i = 0;
    while (test_len-- && test[i]) {
      if (!f(test[i])) {
        break;
      }
      ++i;
    }
    return i;
  }

  static constexpr auto sscan_eats_white_space = true;

  constexpr inline bool sscan(const char *test, size_t test_len, const char *format) noexcept {
    // strncmp
    while (*format && test_len) {
      if constexpr (sscan_eats_white_space) {
        if (is_white_space(*format) || is_white_space(*test)) {
          size_t len = eat_while(test, test_len, is_white_space);
          test += len;
          test_len -= len;
          format += eat_while(format, -1, is_white_space);
          continue;
        }
      }
      if (*format++ != *test++) {
        return false;
      }
      --test_len;
    }

    if constexpr (sscan_eats_white_space) {
      format += eat_while(format, -1, is_white_space);
      test_len -= eat_while(test, test_len, is_white_space);
    }

    return !*format && !test_len;
  }

  /**
   * checks the input string (test) compares the same as the format string, and writes down matched
   * values ({}) to provided variable references.
   * 
   * returns true if the input string matches the format string, false otherwise.
   */
  template<class Arg0, class ...Args>
  constexpr inline bool sscan(const char *test, size_t test_len, const char *format, Arg0 &arg0, Args &...args) noexcept {
    while (*format && test_len) {
      if constexpr (sscan_eats_white_space) {
        if (is_white_space(*format) || is_white_space(*test)) {
          size_t len = eat_while(test, test_len, is_white_space);
          test += len;
          test_len -= len;
          format += eat_while(format, -1, is_white_space);
          continue;
        }
      }

      if (*format == '{' && format[1] == '}') {
        using Decay = std::decay_t<Arg0>;
        // int, uint, ...
        if constexpr (std::is_integral_v<Decay> && !std::is_same_v<Decay, char> && !std::is_same_v<Decay, unsigned char>) {
          size_t i;
          if (std::is_signed_v<Decay> && test_len && *test == '-') {
            i = 1 + eat_while(test + 1, test_len - 1, is_digit);
          } else {
            i = eat_while(test, test_len, is_digit);
          }
          auto result = ::etl::to_arithmetic<Decay>(::etl::string_view(test, i));
          if (result) {
            arg0 = result.value();
            return sscan(test + i, test_len - i, format + 2, args...);
          } else {
            return false;
          }
        } else if constexpr (std::is_same_v<Decay, char> || std::is_same_v<Decay, unsigned char>) {
          // char
          if (test_len) {
            arg0 = *test++;
            return sscan(test + 1, test_len - 1, format + 2, args...);
          } else {
            return false;
          }
        } else if constexpr (std::is_pointer_v<Decay> && std::is_same_v<std::remove_pointer_t<Decay>, char>) {
          // char *
          size_t i = eat_while(test, test_len, is_non_white_space);
          if (i) {
            // if arg0 is array reference, then its size is known
            using NoRef = std::remove_reference_t<Arg0>;
            size_t safe = i;
            if constexpr (std::is_array_v<NoRef>) {
              safe = safe < (std::extent_v<NoRef> - 1) ? safe : (std::extent_v<NoRef> - 1);
              arg0[safe] = '\0';
            }
            for (size_t n = 0; n < safe; ++n) {
              arg0[n] = test[n];
            }
            return sscan(test + i, test_len - i, format + 2, args...);
          } else {
            return false;
          }
        } else if constexpr (is_etl_string<Decay>::value) {
          // ::etl::string
          size_t i = eat_while(test, test_len, is_non_white_space);
          if (i) {
            arg0.assign(test, i);
            return sscan(test + i, test_len - i, format + 2, args...);
          } else {
            return false;
          }
        } else if constexpr (std::is_floating_point_v<Decay>) {
          // note: floats can only be parsed when it is not surrounded by other nws characters
          size_t i = eat_while(test, test_len, is_non_white_space);
          if (i) {
            auto result = ::etl::to_arithmetic<Decay>(::etl::string_view(test, i));
            if (result) {
              arg0 = result.value();
              return sscan(test + i, test_len - i, format + 2, args...);
            } else {
              return false;
            }
          } else {
            return false;
          }
        } else {
          // unsupported type
          __builtin_unreachable();
        }
      }
      if (*format++ != *test++) {
        return false;
      }
      --test_len;
    }

    if constexpr (sscan_eats_white_space) {
      format += eat_while(format, -1, is_white_space);
      test_len -= eat_while(test, test_len, is_white_space);
    }

    return !*format && !test_len;
  }

  // this overload is not useful in practice
  /*template<size_t N, class ...Args>
  constexpr inline bool sscan(const char (&test)[N], const char *format, Args &...args) noexcept {
    return sscan(test, N - 1, format, args...);
  }*/

  template<class ...Args>
  constexpr inline bool sscan(::etl::string_view test, const char *format, Args &...args) noexcept {
    return sscan(test.data(), test.size(), format, args...);
  }

}  // namespace troll
