#pragma once

#include "tasking.hpp"

namespace kernel {
int activate_task(volatile context_t* kernel_context, task_descriptor* current_task);
void initialize();
void enable_dcache();
void enable_bcache();
void enable_icache();
}  // namespace kernel
