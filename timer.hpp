#pragma once

#include "kstddefs.hpp"

namespace kernel {

class timer {
public:
  timer() : next_tick{0} {}
  void initialize();
  void rearm_timer_interrupt();

  static uint32_t read_current_tick();
private:
  uint32_t next_tick;
  const uint32_t tick_interval = TIMER_INTERRUPT_INTERVAL;

  void set_timer_interrupt(uint32_t target_tick);
};

} // namespace kernel
