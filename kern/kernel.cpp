#include "kernel.hpp"
#include "irq.hpp"
#include "rpi.hpp"
#include "gpio.hpp"

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

void handle_timer_interrupt(task_manager& the_task_manager, timer& the_timer) {
  the_timer.rearm_timer_interrupt();
  the_task_manager.wake_up_tasks_on_event(events_t::TIMER, 1);
}

void handle_gpio_interrupt(
  task_manager& the_task_manager,
  gpio::GPIO_IRQ_TYPE interrupt_type,
  size_t uart_channel,
  gpio::uart_interrupt_state& uart_irq_state
) {
  bool is_channel_0 = uart_channel == 0;

  switch (interrupt_type) {
  case gpio::GPIO_IRQ_TYPE::RX_TIMEOUT:
  case gpio::GPIO_IRQ_TYPE::RHR: {
    the_task_manager.wake_up_tasks_on_event(is_channel_0 ? events_t::UART_R0 : events_t::UART_R1, 1);
    uart_irq_state.disable_rx(uart_channel);
    break;
  }
  case gpio::GPIO_IRQ_TYPE::THR: {
    the_task_manager.wake_up_tasks_on_event(is_channel_0 ? events_t::UART_T0 : events_t::UART_T1, 1);
    uart_irq_state.disable_tx(uart_channel);
    break;
  }
  case gpio::GPIO_IRQ_TYPE::MODEM_STATUS: {
    // only relevant for keeping track of cts for merklin
    if (!is_channel_0) {
      // read the modem status
      the_task_manager.wake_up_tasks_on_event(events_t::CTS_1, uart_read_register(0, 1, rpi::UART_MSR));
      // uart_irq_state.disable_modem_interrupt();
    }
    break;
  }
  default:
    break;
  }
}

void handle_gpio_interrupt(task_manager& the_task_manager, gpio::uart_interrupt_state& uart_irq_state) {
  uint32_t gpeds = read_gpeds();
  if (gpeds & (1 << 24)) {
    auto interrupt_type = gpio::get_interrupt_type(0, 0);
    if (interrupt_type != gpio::GPIO_IRQ_TYPE::NONE) {
      handle_gpio_interrupt(the_task_manager, interrupt_type, 0, uart_irq_state);
    } else {
      interrupt_type = gpio::get_interrupt_type(0, 1);
      if (interrupt_type != gpio::GPIO_IRQ_TYPE::NONE) {
        handle_gpio_interrupt(the_task_manager, interrupt_type, 1, uart_irq_state);
      }
    }
  }
  set_gpeds(gpeds);
}

void handle_interrupt(task_manager& the_task_manager, timer& the_timer, gpio::uart_interrupt_state& uart_irq_state) {
  uint32_t iar = irq::read_interrupt_iar();
  uint32_t irq_id = irq::get_irq_id(iar);
  if (irq::is_timer_interrupt(irq_id)) {
    handle_timer_interrupt(the_task_manager, the_timer);
  } else if (irq::is_gpio_interrupt(irq_id)) {
    handle_gpio_interrupt(the_task_manager, uart_irq_state);
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
