#include "kernel.hpp"
#include "rpi.hpp"
#include "tasking.hpp"
#include "user.hpp"

void initialize() {
  // run static/global constructors manually
  using constructor_t = void (*)();
  extern constructor_t __init_array_start[], __init_array_end[];
  for (auto* fn = __init_array_start; fn < __init_array_end; (*fn++)())
    ;
}

// bits[0:24] hold N in svc N
#define ESR_MASK 0x1FFFFFF

int main() {
  initialize();
  init_gpio();
  init_spi(0);
  init_uart(0);

  // sets up the vector exception table
  kernel::initialize();
  kernel::task_manager task_manager;
  kernel::context_t kernel_context;

  uart_puts(0, 0, SC_CLRSCR, LEN_LITERAL(SC_CLRSCR));

  // spawn first task
  auto *current_task = task_manager.new_task(KERNEL_TID, 1, PRIORITY_L3);
  current_task->context.registers[0] = reinterpret_cast<int64_t>(k2_user_task);
  current_task->context.exception_lr = reinterpret_cast<uint64_t>(task_wrapper);
  task_manager.ready_push(current_task);

  uint32_t esr_el1, request;
  while ((current_task = task_manager.get_task())) {
    current_task->state = kernel::task_state_t::Active;
    esr_el1 = kernel::activate_task(&kernel_context, current_task);
    request = esr_el1 & ESR_MASK;

    switch (request) {
      case SYSCALLN_CREATE: {
        task_manager.k_create(current_task);
        break;
      }
      case SYSCALLN_MYTID: {
        task_manager.k_my_tid(current_task);
        break;
      }
      case SYSCALLN_MYPARENTTID: {
        task_manager.k_my_parent_tid(current_task);
        break;
      }
      case SYSCALLN_YIELD: {
        task_manager.k_yield(current_task);
        break;
      }
      case SYSCALLN_SEND: {
        task_manager.k_send(current_task);
        break;
      }
      case SYSCALLN_RECEIVE: {
        task_manager.k_receive(current_task);
        break;
      }
      case SYSCALLN_REPLY: {
        task_manager.k_reply(current_task);
        break;
      }
      case SYSCALLN_EXIT: {
        task_manager.k_exit(current_task);
        break;
      }
      default: {
        // in case the kernel does not recognize the request, print it and return
        uart_puts(0, 0, "Got request: ", 13);
        for (int i = 0; i < 32; ++i) {
          uart_putc(0, 0, '0' + (esr_el1 & 1));
          esr_el1 >>= 1;
        }
        uart_puts(0, 0, "\r\n", 2);
        return 0;
      }
    }
  }
  uart_puts(0, 0, "Finished processing all tasks!\r\n", 32);
  return 0;
}
