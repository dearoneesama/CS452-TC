#pragma once

#include "kstddefs.hpp"

namespace irq {

void initialize_irq();
uint32_t read_interrupt_iar();
uint32_t get_irq_id(uint32_t iar);
void end_interrupt(uint32_t iar);
bool is_timer_interrupt(uint32_t irq_id);
bool is_gpio_interrupt(uint32_t irq_id);

} // namespace irq
