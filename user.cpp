#include "user.hpp"

#include "rpi.hpp"
#include "user_syscall.h"
#include "format.hpp"
#include "servers.hpp"

namespace k1 {

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

}  // namespace k1

void task_wrapper(void (*the_task)()) {
  the_task();
  Exit();
}

namespace k2 {

void test_child_task() {
  uart_puts(0, 0, "Hello from child\r\n", 18);

  RegisterAs("k2-child-task");
  int tid = MyTid();
  int ptid = MyParentTid();
  if (ptid & EXITED_PARENT_MASK) {
    ptid -= EXITED_PARENT_MASK;
  }

  if (WhoIs("k2-user-task") != ptid) {
    uart_puts(0, 0, "Failed lookup of k2-user-task...\r\n", 34);
    return;
  }

  if (WhoIs("k2-child-task") != tid) {
    uart_puts(0, 0, "Failed lookup of k2-child-task...\r\n", 35);
    return;
  }

  // now overwrite registration
  RegisterAs("k2-user-task");
  if (WhoIs("k2-user-task") != tid) {
    uart_puts(0, 0, "Failed lookup of k2-user-task after overwrite...\r\n", 50);
    return;
  }

  uart_puts(0, 0, "Success from child too!\r\n", 25);
  return;
}

void test_user_task() {
  int nameserver_task = Create(PRIORITY_L5, nameserver);
  Create(PRIORITY_L1, test_child_task);
  char buffer[32];
  uart_puts(0, 0, buffer, troll::snformat(buffer, "Nameserver TID: {}\r\n", nameserver_task));

  int err = RegisterAs("k2-user-task");
  if (!err) {
    int resp = WhoIs("k2-user-task");
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

etl::string_view message_to_rpc(char m) {
  switch (static_cast<RPC_MESSAGE>(m)) {
    case RPC_MESSAGE::PAPER:
      return "paper";
    case RPC_MESSAGE::ROCK:
      return "rock";
    case RPC_MESSAGE::SCISSORS:
      return "scissors";
    default:
      return "unknown";
  }
}

void rpc_other_user_task() {
  auto rpc_server = WhoIs("rpcserver");
  auto tid = MyTid();
  char reply;
  while (1) {
    loop:
    // play two rounds and quit
    auto message = static_cast<char>(RPC_MESSAGE::SIGNUP);
    Send(rpc_server, &message, 1, &reply, 1);

    auto *timer = reinterpret_cast<unsigned *>(0xfe003000 + 0x04);
    for (size_t i = 0; i < 2; ++i) {
      message = *timer % 3;  // the action
      Send(rpc_server, &message, 1, &reply, 1);
      char buf[100];
      size_t len = troll::snformat(buf, "[{}, {}] ", tid, message_to_rpc(message));

      switch (static_cast<RPC_REPLY>(reply)) {
        case RPC_REPLY::ABANDONED:
          len += troll::snformat(buf + len, sizeof buf - len, "Match abandoned.", tid);
          goto loop;
        case RPC_REPLY::WIN_AND_SEND_ACTION:
          len += troll::snformat(buf + len, sizeof buf - len, "I won.", tid);
          break;
        case RPC_REPLY::LOSE_AND_SEND_ACTION:
          len += troll::snformat(buf + len, sizeof buf - len, "I lost.", tid);
          break;
        case RPC_REPLY::TIE_AND_SEND_ACTION:
          len += troll::snformat(buf + len, sizeof buf - len, "Tie.", tid);
          break;
        default:
          break;
      }
      len += troll::snformat(buf + len, sizeof buf - len, "\r\n");
      uart_puts(0, 0, buf, len);
      uart_getc(0, 0);
    }
    message = static_cast<char>(RPC_MESSAGE::QUIT);
    Send(rpc_server, &message, 1, &reply, 1);
  }
}

/**
 * spawns an rpc server and 5 clients; let them play games on their own.
 */
void rpc_first_user_task() {
  Create(PRIORITY_L5, nameserver);
  Create(PRIORITY_L4, rpcserver);
  DEBUG_LITERAL("You need to press a key to continue outputs.\r\n");
  for (size_t i = 0; i < 5; ++i) {
    Create(PRIORITY_L2, rpc_other_user_task);
  }
}

}  // namespace k2
