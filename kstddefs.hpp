#pragma once

#include <cstddef>
#include <cstdint>

// tasking configurations
using tid_t = uint32_t;

enum priority_t {
  PRIORITY_L0 = 0,  // lowest
  PRIORITY_L1,
  PRIORITY_L2,
  PRIORITY_L3,
  PRIORITY_L4,
  PRIORITY_L5,
  PRIORITY_UNDEFINED,
};

static constexpr size_t NUM_PRIORITIES = PRIORITY_UNDEFINED;

static constexpr size_t TASK_STACK_SIZE/*_BYTES*/ = 2048;
static constexpr size_t MAX_NUM_TASKS = 50;

static constexpr size_t SP_ALIGNMENT = 16;

static constexpr tid_t KERNEL_TID = 1;
static constexpr tid_t STARTING_TASK_TID = 2;

static_assert(!(TASK_STACK_SIZE % SP_ALIGNMENT));

#define EXITED_PARENT_MASK (1 << 31)

#include "user_syscall.include"

// screen-printing utilities
#define LEN_LITERAL(x) (sizeof(x) / sizeof(x[0]) - 1)

#define SC_CLRSCR "\033[1;1H\033[2J"  // clear entire screen
#define SC_MOVSCR "\033[;H"           // move cursor to the top
#define SC_CLRLNE "\033[2K\r"         // clear current line
#define SC_HIDCUR "\033[?25l"         // hide cursor
#define SC_SHWCUR "\033[?25h"         // display cursor
