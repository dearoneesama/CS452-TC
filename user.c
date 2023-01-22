#include "user.h"

#include "rpi.h"
#include "user_syscall.h"

void first_user_function() {
  unsigned int val;
  asm volatile("mov %[zzz], x0" : [zzz] "=r"(val));
  char first_digit_v = val / 100;
  char second_digit_v = (val - (first_digit_v * 100)) / 10;
  char third_digit_v = val % 10;

  uart_putc(0, 0, '0' + first_digit_v);
  uart_putc(0, 0, '0' + second_digit_v);
  uart_putc(0, 0, '0' + third_digit_v);

  uart_puts(0, 0, "hello", 5);
  int result = my_sys_call();
  char first_digit = result / 100;
  char second_digit = (result - (first_digit * 100)) / 10;
  char third_digit = result % 10;

  uart_putc(0, 0, '0' + first_digit);
  uart_putc(0, 0, '0' + second_digit);
  uart_putc(0, 0, '0' + third_digit);
}
