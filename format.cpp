#include "format.hpp"

namespace {
  void pad_left(char *dest, size_t destlen, const char *src, size_t srclen, char padchar) {
    for (size_t i = 0; i < srclen; ++i) {
      dest[i] = src[i];
    }
    for (size_t i = srclen; i < destlen; ++i) {
      dest[i] = padchar;
    }
  }

  void pad_middle(char *dest, size_t destlen, const char *src, size_t srclen, char padchar) {
    size_t left = (destlen - srclen) / 2;
    for (size_t i = 0; i < left; ++i) {
      dest[i] = padchar;
    }
    for (size_t i = left; i < left + srclen; ++i) {
      dest[i] = src[i - left];
    }
    for (size_t i = left + srclen; i < destlen; ++i) {
      dest[i] = padchar;
    }
  }

  void pad_right(char *dest, size_t destlen, const char *src, size_t srclen, char padchar) {
    for (size_t i = 0; i < destlen - srclen; ++i) {
      dest[i] = padchar;
    }
    for (size_t i = destlen - srclen; i < destlen; ++i) {
      dest[i] = src[i - destlen + srclen];
    }
  }
}

namespace troll {

  void pad(char *dest, size_t destlen, const char *src, size_t srclen, padding p, char padchar) {
    if (destlen < srclen) {
      srclen = destlen;
    }
    switch (p) {
      case padding::left:
        return pad_left(dest, destlen, src, srclen, padchar);
      case padding::middle:
        return pad_middle(dest, destlen, src, srclen, padchar);
      case padding::right:
        return pad_right(dest, destlen, src, srclen, padchar);
    }
  }

}  // namespace troll
