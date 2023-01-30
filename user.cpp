#include "user.hpp"

#include "rpi.hpp"
#include "user_syscall.h"
#include "format.hpp"
#include "servers.hpp"

void other_task() {
  int tid = MyTid();
  int ptid = MyParentTid();
  char buffer[50];
  size_t len = 0;

  if (ptid & EXITED_PARENT_MASK) {
    len = troll::snformat(buffer, "My TID is {} and my exited parent's TID was {}\r\n", tid, ptid - EXITED_PARENT_MASK);
  } else {
    len = troll::snformat(buffer, "My TID is {} and my parent's TID is {}\r\n", tid, ptid);
  }
  uart_puts(0, 0, buffer, len);
  Yield();
  uart_puts(0, 0, buffer, len);
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
}

void task_wrapper(void (*the_task)()) {
  the_task();
  Exit();
}

void k2_child_task() {
  uart_puts(0, 0, "Hello from child\r\n", 18);

  RegisterAs("k2-child-task\0");
  int tid = MyTid();
  int ptid = MyParentTid();
  if (ptid & EXITED_PARENT_MASK) {
    ptid -= EXITED_PARENT_MASK;
  }

  if (WhoIs("k2-user-task\0") != ptid) {
    uart_puts(0, 0, "Failed lookup of k2-user-task...\r\n", 34);
    return;
  }

  if (WhoIs("k2-child-task\0") != tid) {
    uart_puts(0, 0, "Failed lookup of k2-child-task...\r\n", 35);
    return;
  }

  // now overwrite registration
  RegisterAs("k2-user-task\0");
  if (WhoIs("k2-user-task\0") != tid) {
    uart_puts(0, 0, "Failed lookup of k2-user-task after overwrite...\r\n", 50);
    return;
  }

  uart_puts(0, 0, "Success from child too!\r\n", 25);
  return;
}

void k2_user_task() {
  int nameserver_task = Create(PRIORITY_L5, nameserver);
  Create(PRIORITY_L1, k2_child_task);
  char buffer[32];
  uart_puts(0, 0, buffer, troll::snformat(buffer, "Nameserver TID: {}\r\n", nameserver_task));

  int err = RegisterAs("k2-user-task\0");
  if (!err) {
    int resp = WhoIs("k2-user-task\0");
    if (resp != 2) {
      uart_puts(0, 0, buffer, troll::snformat(buffer, "Got this: {}\r\n", resp));
      return;
    }
    uart_puts(0, 0, "Success, correctly got 2!\r\n", 27);
    return;
  } else {
    uart_puts(0, 0, "Screwed up registration\r\n", 25);
    return;
  }
}
