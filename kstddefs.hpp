#pragma once

#include <cstddef>
#include <cstdint>

// tasking configurations
using tid_t = uint32_t;

enum priority_t {
  PRIORITY_IDLE = 0, // only for idle
  PRIORITY_L10,
  PRIORITY_L9,
  PRIORITY_L8,
  PRIORITY_L7,
  PRIORITY_L6,
  PRIORITY_L5,
  PRIORITY_L4,
  PRIORITY_L3,
  PRIORITY_L2,
  PRIORITY_L1,
  PRIORITY_L0, // highest priority
  PRIORITY_UNDEFINED,
};

struct time_distribution_t {
  uint64_t total_ticks;
  uint64_t kernel_ticks;
  uint64_t idle_ticks;
};

static constexpr size_t NUM_PRIORITIES = PRIORITY_UNDEFINED;

static constexpr size_t TASK_STACK_SIZE/*_BYTES*/ = 1024 * 1024;
static constexpr size_t MAX_NUM_TASKS = 50;

static constexpr size_t SP_ALIGNMENT = 16;

static constexpr tid_t KERNEL_TID = 1;
static constexpr tid_t STARTING_TASK_TID = 2;
static constexpr tid_t ENDING_TASK_TID = STARTING_TASK_TID + MAX_NUM_TASKS;

static_assert(!(TASK_STACK_SIZE % SP_ALIGNMENT));

#define EXITED_PARENT_MASK (1 << 31)

static constexpr size_t MAX_NUM_EVENTS = 1;

#include "user_syscall.include"

// hardware
#define GET_TIMER_COUNT() (*reinterpret_cast<volatile unsigned *>(0xfe003000 + 0x04))

static constexpr unsigned TIMER_FREQ = 1'000'000;  // 1mhz
static constexpr unsigned NUM_TICKS_IN_1US = 1'000'000 / TIMER_FREQ; // microseconds
static constexpr unsigned NUM_TICKS_IN_1MS = NUM_TICKS_IN_1US * 1000; // milliseconds
// DEBUG: change this
static constexpr unsigned TIMER_INTERRUPT_INTERVAL = 10 * NUM_TICKS_IN_1MS; // 10ms
