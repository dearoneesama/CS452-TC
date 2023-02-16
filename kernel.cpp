#include "kernel.hpp"
#include "irq.hpp"

extern "C" void initialize_kernel();
extern "C" int kernel_to_task(volatile kernel::context_t* kernel_context, volatile kernel::context_t* task_context);

namespace kernel {
int activate_task(volatile context_t* kernel_context, task_descriptor* current_task) {
  return kernel_to_task(kernel_context, &(current_task->context));
}

void initialize() {
  initialize_kernel();
  irq::initialize_irq();
}

void handle_interrupt(task_manager& the_task_manager, timer& the_timer) {
  uint32_t iar = irq::read_interrupt_iar();
  uint32_t irq_id = irq::get_irq_id(iar);
  if (irq::is_timer_interrupt(irq_id)) {
    the_timer.rearm_timer_interrupt();
    the_task_manager.wake_up_tasks_on_event(0, 1);
  }
  irq::end_interrupt(iar);
}

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
