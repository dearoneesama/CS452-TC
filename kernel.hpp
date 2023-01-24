#pragma once

#include "tasking.hpp"

namespace kernel {
int activate_task(context_t* kernel_context, task_descriptor* current_task);
void initialize();
}  // namespace kernel
