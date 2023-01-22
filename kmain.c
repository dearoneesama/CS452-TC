#include "kernel.h"
#include "rpi.h"
#include "user.h"

int kmain() {
  init_gpio();
  init_spi(0);
  init_uart(0);

  unsigned int val = initialize(2);

  uart_putc(0, 0, '0' + val);
  uart_puts(0, 0, "\r\n", 2);
  int* sp = make_stack();
  if (((long long)sp % 16) != 0) {
    uart_puts(0, 0, "unaligned\r\n", 11);
  }
  int spsr = 0;

  char msg1[] = "Hello world, this is kmain (" __TIME__ ")\r\nPress 'q' to reboot\r\n";
  uart_puts(0, 0, msg1, sizeof(msg1) - 1);
  char prompt[] = "PI> ";
  uart_puts(0, 0, prompt, sizeof(prompt) - 1);

  while (1) {
    int a = activate(spsr, first_user_function, sp + 32);
    if (a == 1) {
      uart_puts(0, 0, "success", 7);
      return 0;
    } else {
      uart_puts(0, 0, "got ", 4);
      int b;
      asm volatile("MRS x0, ESR_EL1");
      asm volatile("mov %[zzz], x0" : [zzz] "=r"(b));
      for (int i = 0; i < 32; i++) {
        uart_putc(0, 0, '0' + (b & 1));
        b >>= 1;
      }
      return 0;
    }
  }
  return 0;
}
