#pragma once

#include <stdint.h>

/**
 * Align everything to 64bit for easier time on the assembler side
 */
struct context_t {
  // x0 to x30
  int64_t registers[31];

  uint64_t spsr;
  uint64_t stack_pointer;
  uint64_t exception_lr;
};
