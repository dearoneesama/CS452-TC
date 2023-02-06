#include "kernel.hpp"

extern "C" void initialize_kernel();
extern "C" int kernel_to_task_r(volatile kernel::context_t* kernel_context, volatile kernel::context_t* task_context);

namespace kernel {
int activate_task(volatile context_t* kernel_context, task_descriptor* current_task) {
  return kernel_to_task_r(kernel_context, &(current_task->context));
}

void initialize() { initialize_kernel(); }

void enable_dcache() {
  asm volatile("msr SCTLR_EL1, %x0\n\t" :: "r"(1 << 2));
  asm volatile("IC IALLUIS");
}

void enable_bcache() {
  asm volatile("msr SCTLR_EL1, %x0\n\t" :: "r"((1 << 2) | (1 << 12)));
}

void enable_icache() {
  asm volatile("msr SCTLR_EL1, %x0\n\t" :: "r"(1 << 12));
}
}  // namespace kernel
