#include "user.hpp"

#include "rpi.hpp"
#include "user_syscall.h"
#include "format.hpp"

void other_task() {
  int tid = MyTid();
  int ptid = MyParentTid();
  char buffer[50];
  auto len = troll::snformat(buffer, "My TID is {} and my parent's TID is {}\r\n", tid, ptid);
  uart_puts(0, 0, buffer, len);
  Yield();
  uart_puts(0, 0, buffer, len);
  Exit();
}

/**
 * Creates two tasks at priority L1, followed by two tasks of priority P4
 * 
 * In terms of termination:
 *  - Each L4 task will run until completion before anything else
 *  - First user task will exit after each L4 task has terminated
 *  - The first L1 will print one line, and yield
 *  - The second L1 will print one line, and yield
 *  - The first L1 finishes
 *  - The second L1 finishes
*/
void first_user_task() {
  // current task is PRIORITY_L3

  const char *format = "Created: {}\r\n";
  char buffer[20];

  // lower priority, meaning lower n in Ln
  int task_1 = Create(PRIORITY_L1, other_task);
  uart_puts(0, 0, buffer, troll::snformat(buffer, format, task_1));
  int task_2 = Create(PRIORITY_L1, other_task);
  uart_puts(0, 0, buffer, troll::snformat(buffer, format, task_2));

  // higher priority, meaning higher n in Ln
  int task_3 = Create(PRIORITY_L4, other_task);
  uart_puts(0, 0, buffer, troll::snformat(buffer, format, task_3));
  int task_4 = Create(PRIORITY_L4, other_task);
  uart_puts(0, 0, buffer, troll::snformat(buffer, format, task_4));

  uart_puts(0, 0, "FirstUserTask: exiting\r\n", 24);
  Exit();
}
