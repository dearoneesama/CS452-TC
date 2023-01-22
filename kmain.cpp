#include "kernel.hpp"
#include "kernel_syscall.hpp"
#include "ready_lists.hpp"
#include "rpi.hpp"
#include "task_descriptor.hpp"
#include "task_list.hpp"
#include "user.hpp"

void initialize() {
  // run static/global constructors manually
  using constructor_t = void (*)();
  extern constructor_t __init_array_start[], __init_array_end[];
  for (auto* fn = __init_array_start; fn < __init_array_end; (*fn++)())
    ;
}

namespace std {
void __throw_length_error(char const*) { __builtin_unreachable(); }
}  // namespace std

#define TASK_STACK_SIZE_IN_BYTES 512
#define MAX_NUM_TASKS 50
// bits[0:24] hold N in svc N
#define ESR_MASK 0x1FFFFFF

#ifndef DEBUG
#define DEBUG 0
#endif

int main() {
  initialize();
  init_gpio();
  init_spi(0);
  init_uart(0);

  // sets up the vector exception table
  kernel::initialize();

  uart_puts(0, 0, "\033[2J", 4);

  // stack pointer must be 16bytes aligned
  // add 8 in case we need to "fix" the stack pointer
  char stacks[(TASK_STACK_SIZE_IN_BYTES * MAX_NUM_TASKS) + 8];
  char* sp = stacks;

  // this is a short-term hack, maybe rewrite it in assembly and align it there
  char remainder = (uint64_t)sp % 16;
  if (remainder != 0) {
#if DEBUG > 0
    uart_puts(0, 0, "Unaligned task stack pointer: ", 30);
    uart_putui64(0, 0, (uint64_t)sp);
    uart_puts(0, 0, "\r\nRemainder: ", 13);
    uart_putui64(0, 0, remainder);
    uart_puts(0, 0, "\r\n", 2);
#endif

    // sp % 16 can be either 4 or 8
    sp += 16 - remainder;

#if DEBUG > 0
    uart_puts(0, 0, "New task stack pointer: ", 24);
    uart_putui64(0, 0, (uint64_t)sp);
    uart_puts(0, 0, "\r\nRemainder: ", 13);
    uart_putui64(0, 0, (uint64_t)sp % 16);
    uart_puts(0, 0, "\r\n", 2);
#endif
  }

  context_t kernel_context;
  task_descriptor all_tasks[MAX_NUM_TASKS];

  ready_lists rl;
  task_list free_list;

  for (int i = 0; i < MAX_NUM_TASKS; ++i) {
    task_descriptor& td = all_tasks[i];
    free_list.push(&all_tasks[i]);
    td.context.stack_pointer = (uint64_t)(sp + (i + 1) * TASK_STACK_SIZE_IN_BYTES);
    // tid starts at 2 for tasks
    td.tid = 2 + i;
  }

  // TODO: put this in a function?
  task_descriptor* current_task = free_list.pop();
  current_task->parent_tid = 1;
  current_task->priority = P2;
  current_task->context.registers[0] = 452;  // this is only for debugging
  current_task->context.exception_lr = (uint64_t)first_user_task;
  current_task->state = Ready;
  rl.push(current_task);

  uint32_t esr_el1, request;
  while (1) {
    if (rl.empty()) {
      break;
    }

    // TODO: put this in a `schedule(...)` function
    current_task = rl.pop();
    current_task->state = Active;

    esr_el1 = kernel::activate_task(&kernel_context, current_task);
    request = esr_el1 & ESR_MASK;

    // TODO: enum for this?
    switch (request) {
      case 1: {
        kernel::kCreate(current_task, &free_list, &rl);
        break;
      }
      case 2: {
        kernel::kMyTid(current_task, &rl);
        break;
      }
      case 3: {
        kernel::kMyParentTid(current_task, &rl);
        break;
      }
      case 4: {
        kernel::kYield(current_task, &rl);
        break;
      }
      case 5: {
        kernel::kExit(current_task, &free_list);
        break;
      }
      default: {
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
