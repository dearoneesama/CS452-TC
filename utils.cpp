#include "utils.hpp"

namespace utils {
void uint32_to_buffer(char* buffer, uint32_t n) {
  buffer[0] = n;
  buffer[1] = n >> 8;
  buffer[2] = n >> 16;
  buffer[3] = n >> 24;
}

uint32_t buffer_to_uint32(char* buffer) {
  return buffer[0] + (buffer[1] << 8) + (buffer[2] << 16) + (buffer[3] << 24);
}
}
