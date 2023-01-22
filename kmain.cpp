#include "kernel.h"
#include "rpi.hpp"
#include "user.h"

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

#define TASK_STACK_SIZE_IN_16B 128
#define MAX_NUM_TASKS 50
// bits[0:24] hold N in svc N
#define ESR_MASK 0x1FFFFFF

#ifndef DEBUG
#define DEBUG 1
#endif

int main() {
  initialize();
  init_gpio();
  init_spi(0);
  init_uart(0);

  // sets up the vector exception table
  initialize_kernel();

  uart_puts(0, 0, "\033[2J", 4);

  // stack pointer must be 16bytes aligned
  // add 1 in case we need to "fix" the stack pointer
  uint64_t stacks[(TASK_STACK_SIZE_IN_16B * MAX_NUM_TASKS) + 1];
  uint64_t* sp = stacks;

  // this is a short-term hack, maybe rewrite it in assembly and align it there
  char remainder = (uint64_t)sp % 16;
  if (remainder != 0) {

#if DEBUG > 0
    uart_puts(0, 0, "Unaligned task stack pointer: ", 30);
    uart_putui64(0, 0, (uint64_t)sp);
    uart_puts(0, 0, "\r\n Remainder: ", 14);
    uart_putui64(0, 0, remainder);
    uart_puts(0, 0, "\r\n", 2);
#endif

    // sp % 16 can be either 4 or 8
    sp = (uint64_t*)(((char *) sp) + (16 - remainder));

#if DEBUG > 0
    uart_puts(0, 0, "New task stack pointer: ", 24);
    uart_putui64(0, 0, (uint64_t)sp);
    uart_puts(0, 0, "\r\n Remainder: ", 14);
    uart_putui64(0, 0, (uint64_t)sp % 16);
    uart_puts(0, 0, "\r\n", 2);
#endif
  }

  int spsr = 0;

  char msg1[] = "Hello world, this is kmain (" __TIME__ ")\r\nPress 'q' to reboot\r\n";
  uart_puts(0, 0, msg1, sizeof(msg1) - 1);
  char prompt[] = "PI> ";
  uart_puts(0, 0, prompt, sizeof(prompt) - 1);

  uint32_t esr_el1, request;
  while (1) {
    esr_el1 = activate(spsr, first_user_function, sp + 32);
    request = esr_el1 & ESR_MASK;
    uart_puts(0, 0, "Got request: ", 13);
    uart_putc(0, 0, '0' + request);
    uart_puts(0, 0, "\r\n", 2);
    return 0;
  }
  return 0;
}
