#pragma once

#include "tasking.hpp"
#include "timer.hpp"
#include "gpio.hpp"

namespace kernel {
int activate_task(volatile context_t* kernel_context, task_descriptor* current_task);
void initialize();
void handle_interrupt(task_manager& the_task_manager, timer& the_timer, gpio::uart_interrupt_state& uart_irq_state);
void enable_dcache();
void enable_bcache();
void enable_icache();
}  // namespace kernel
