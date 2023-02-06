#include "kernel.hpp"

extern "C" void initialize_kernel();
extern "C" int kernel_to_task_r(volatile kernel::context_t* kernel_context, volatile kernel::context_t* task_context);

namespace kernel {
int activate_task(volatile context_t* kernel_context, task_descriptor* current_task) {
  return kernel_to_task_r(kernel_context, &(current_task->context));
}

void initialize() { initialize_kernel(); }
}  // namespace kernel
