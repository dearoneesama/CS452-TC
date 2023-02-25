#include "format.hpp"

namespace {
  void pad_left(char *dest, size_t dest_pad_len, const char *src, size_t srclen, char padchar) {
    for (size_t i = 0; i < srclen; ++i) {
      dest[i] = src[i];
    }
    for (size_t i = srclen; i < dest_pad_len; ++i) {
      dest[i] = padchar;
    }
  }

  void pad_middle(char *dest, size_t dest_pad_len, const char *src, size_t srclen, char padchar) {
    size_t left = (dest_pad_len - srclen) / 2;
    for (size_t i = 0; i < left; ++i) {
      dest[i] = padchar;
    }
    for (size_t i = left; i < left + srclen; ++i) {
      dest[i] = src[i - left];
    }
    for (size_t i = left + srclen; i < dest_pad_len; ++i) {
      dest[i] = padchar;
    }
  }

  void pad_right(char *dest, size_t dest_pad_len, const char *src, size_t srclen, char padchar) {
    for (size_t i = 0; i < dest_pad_len - srclen; ++i) {
      dest[i] = padchar;
    }
    for (size_t i = dest_pad_len - srclen; i < dest_pad_len; ++i) {
      dest[i] = src[i - dest_pad_len + srclen];
    }
  }
}

namespace troll {

  void pad(char *__restrict__ dest, size_t dest_pad_len, const char *__restrict__ src, size_t srclen, padding p, char padchar) {
    if (dest_pad_len < srclen) {
      srclen = dest_pad_len;
    }
    switch (p) {
      case padding::left:
        return pad_left(dest, dest_pad_len, src, srclen, padchar);
      case padding::middle:
        return pad_middle(dest, dest_pad_len, src, srclen, padchar);
      case padding::right:
        return pad_right(dest, dest_pad_len, src, srclen, padchar);
    }
  }

}  // namespace troll
