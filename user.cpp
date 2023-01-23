#include "user.hpp"

#include "rpi.hpp"
#include "user_syscall.h"

void other_task() {
  int tid = MyTid();
  int ptid = MyParentTid();
  uart_puts(0, 0, "My TID is ", 10);
  uart_putui64(0, 0, tid);
  uart_puts(0, 0, " and my parent's TID is ", 24);
  uart_putui64(0, 0, ptid);
  uart_puts(0, 0, "\r\n", 2);
  Yield();
  uart_puts(0, 0, "My TID is ", 10);
  uart_putui64(0, 0, tid);
  uart_puts(0, 0, " and my parent's TID is ", 24);
  uart_putui64(0, 0, ptid);
  uart_puts(0, 0, "\r\n", 2);
  Exit();
}

/**
 * Creates two tasks at priority P3, followed by two tasks of priority P1
 * 
 * In terms of termination:
 *  - Each P1 task will run until completion before anything else
 *  - First user task will exit after each P1 task has terminated
 *  - The first P3 will print one line, and yield
 *  - The second P3 will print one line, and yield
 *  - The first P3 finishes
 *  - The second P3 finishes
*/
void first_user_task() {
  // current task is P2

  // lower priority, meaning higher n in Pn
  int task_1 = Create(P3, other_task);
  uart_puts(0, 0, "Created: ", 9);
  uart_putui64(0, 0, task_1);
  uart_puts(0, 0, "\r\n", 2);
  int task_2 = Create(P3, other_task);
  uart_puts(0, 0, "Created: ", 9);
  uart_putui64(0, 0, task_2);
  uart_puts(0, 0, "\r\n", 2);

  // higher priority, meaning lower n in Pn
  int task_3 = Create(P1, other_task);
  uart_puts(0, 0, "Created: ", 9);
  uart_putui64(0, 0, task_3);
  uart_puts(0, 0, "\r\n", 2);
  int task_4 = Create(P1, other_task);
  uart_puts(0, 0, "Created: ", 9);
  uart_putui64(0, 0, task_4);
  uart_puts(0, 0, "\r\n", 2);

  uart_puts(0, 0, "FirstUserTask: exiting\r\n", 24);
  Exit();
}
