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

etl::string_view message_to_rps(char m) {
  switch (static_cast<RPS_MESSAGE>(m)) {
    case RPS_MESSAGE::PAPER:
      return "paper";
    case RPS_MESSAGE::ROCK:
      return "rock";
    case RPS_MESSAGE::SCISSORS:
      return "scissors";
    case RPS_MESSAGE::QUIT:
      return "quit";
    default:
      return "unknown";
  }
}

// if actions == nullptr: play 0~2 rounds and quit, then sign up again
// otherwise: play actions( and quit)
void rps_other_user_task(RPS_MESSAGE *actions) {
  auto rps_server = WhoIs("rpsserver");
  auto tid = MyTid();
  char reply;
  do {
    auto message = static_cast<char>(RPS_MESSAGE::SIGNUP);
    Send(rps_server, &message, 1, &reply, 1);

    for (size_t i = 0; i < 2; ++i) {
      if (actions) {
        message = static_cast<char>(actions[i]);
      } else {
        if (GET_TIMER_COUNT() % 10 == 7) {  // "random" quit
          message = static_cast<char>(RPS_MESSAGE::QUIT);
        } else {
          message = GET_TIMER_COUNT() % 3;  // "random" action
        }
      }
      Send(rps_server, &message, 1, &reply, 1);

      char buf[100];
      size_t len = troll::snformat(buf, "[{}, {}] ", tid, message_to_rps(message));

      switch (static_cast<RPS_REPLY>(reply)) {
        case RPS_REPLY::ABANDONED:
          len += troll::snformat(buf + len, sizeof buf - len, "Match abandoned.\r\n", tid);
          uart_puts(0, 0, buf, len);
          goto loop;
        case RPS_REPLY::WIN_AND_SEND_ACTION:
          len += troll::snformat(buf + len, sizeof buf - len, "I won.\r\n", tid);
          break;
        case RPS_REPLY::LOSE_AND_SEND_ACTION:
          len += troll::snformat(buf + len, sizeof buf - len, "I lost.\r\n", tid);
          break;
        case RPS_REPLY::TIE_AND_SEND_ACTION:
          len += troll::snformat(buf + len, sizeof buf - len, "Tie.\r\n", tid);
          break;
        default:
          break;
      }
      uart_puts(0, 0, buf, len);
      if (!actions) {
        uart_getc(0, 0);  // block if random play
      }
    }
    message = static_cast<char>(RPS_MESSAGE::QUIT);
    Send(rps_server, &message, 1, &reply, 1);
  loop:
    (void)0;
  } while (!actions);
}

void rps_static_test() {
  // case 1
  DEBUG_LITERAL("Case 1: Client 1 has higher priority than client 2\r\n");
  DEBUG_LITERAL("Client 1 plays rock and client 2 plays scissors, then client 1 quits\r\n");
  // but the rps_static_test() task has lower priority
  Create(PRIORITY_L5, [] { RPS_MESSAGE act[] {RPS_MESSAGE::ROCK, RPS_MESSAGE::QUIT}; rps_other_user_task(act); });
  Create(PRIORITY_L4, [] { RPS_MESSAGE act[] {RPS_MESSAGE::SCISSORS, RPS_MESSAGE::ROCK}; rps_other_user_task(act); } );

  // case 2
  DEBUG_LITERAL("Case 2: Client 1 has higher priority than client 2\r\n");
  DEBUG_LITERAL("Client 1 plays rock and client 2 quits\r\n");
  Create(PRIORITY_L5, [] { RPS_MESSAGE act[] {RPS_MESSAGE::ROCK, RPS_MESSAGE::ROCK}; rps_other_user_task(act); });
  Create(PRIORITY_L4, [] { RPS_MESSAGE act[] {RPS_MESSAGE::QUIT, RPS_MESSAGE::QUIT}; rps_other_user_task(act); } );

  // case 3
  DEBUG_LITERAL("Case 3: Client 1 and client 2 both have the same priority\r\n");
  DEBUG_LITERAL("Client 1 signs up, followed by client 2, then client 1 quits\r\n");
  Create(PRIORITY_L4, [] { RPS_MESSAGE act[] {RPS_MESSAGE::QUIT, RPS_MESSAGE::ROCK}; rps_other_user_task(act); });
  Create(PRIORITY_L4, [] { RPS_MESSAGE act[] {RPS_MESSAGE::QUIT, RPS_MESSAGE::ROCK}; rps_other_user_task(act); } );

  DEBUG_LITERAL("End of edge cases tests\r\n\r\n");
}

/**
 * spawns an rps server and 5 clients; let them play games on their own.
 */
void rps_first_user_task() {
  Create(PRIORITY_L5, nameserver);
  Create(PRIORITY_L4, rpsserver);
  rps_static_test();

  DEBUG_LITERAL("Starting random play using timestamp as the crappy random number.\r\n");
  DEBUG_LITERAL("Five clients are spawned, each of them will sign up, play 0~2 rounds, and re-sign up.\r\n");
  DEBUG_LITERAL("You need to press a key to continue outputs.\r\n");
  for (size_t i = 0; i < 5; ++i) {
    Create(PRIORITY_L2, [] { rps_other_user_task(nullptr); });
  }
}

}  // namespace k2
