#pragma once

#include <cstdint>

namespace utils {
void uint32_to_buffer(char* buffer, uint32_t n);
uint32_t buffer_to_uint32(char* buffer);
}
