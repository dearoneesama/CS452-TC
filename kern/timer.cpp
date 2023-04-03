#include "timer.hpp"

// see broadcom document chapter 10

namespace {
struct SYSTEM_TIMER {
  uint32_t CS;
  uint32_t CLO;
  uint32_t CHI;
  uint32_t C0;
  uint32_t C1;
  uint32_t C2;
  uint32_t C3;
};

static volatile SYSTEM_TIMER* const system_timer = (SYSTEM_TIMER*)(0xfe003000);
};

namespace kernel {

/**
 * This implements a real time loop of sorts:
 * 
 * next_tick = current_tick + interval;
 * while (1) {
 *   interrupt_when(next_tick);
 *   next_tick += interval;
 * }
*/

uint32_t timer::read_current_tick() {
  return system_timer->CLO;
}

void timer::initialize() {
  next_tick = read_current_tick() + tick_interval;
  set_timer_interrupt(next_tick);
}

void timer::rearm_timer_interrupt() {
  system_timer->CS = (1 << 1); // clears the interrupt for C1
  next_tick += tick_interval;
  set_timer_interrupt(next_tick);
}

void timer::set_timer_interrupt(uint32_t target_tick) {
  system_timer->C1 = target_tick;
}
};
