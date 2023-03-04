#include "user.hpp"

#include "rpi.hpp"
#include "user_syscall_typed.hpp"
#include "format.hpp"
#include "servers.hpp"
#include "notifiers.hpp"
#include "format_scan.hpp"
#include "ui.hpp"
#include "merklin.hpp"
#include "gtkterm.hpp"
#include "trains.hpp"

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
 * Creates two tasks at priority P4, followed by two tasks of priority P1
 * 
 * In terms of termination:
 *  - Each P1 task will run until completion before anything else
 *  - First user task will exit after each L4 task has terminated
 *  - The first P4 will print one line, and yield
 *  - The second P4 will print one line, and yield
 *  - The first P4 finishes
 *  - The second P4 finishes
*/
void first_user_task() {
  // current task is PRIORITY_L3

  const char *format = "Created: {}\r\n";
  char buffer[20];

  int task_1 = Create(PRIORITY_L4, other_task);
  uart_puts(0, 0, buffer, troll::snformat(buffer, format, task_1));
  int task_2 = Create(PRIORITY_L4, other_task);
  uart_puts(0, 0, buffer, troll::snformat(buffer, format, task_2));

  int task_3 = Create(PRIORITY_L1, other_task);
  uart_puts(0, 0, buffer, troll::snformat(buffer, format, task_3));
  int task_4 = Create(PRIORITY_L1, other_task);
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
  int nameserver_task = Create(PRIORITY_L1, nameserver);
  Create(PRIORITY_L2, test_child_task);
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

etl::string_view message_to_rps(RPS_MESSAGE m) {
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
  RPS_REPLY reply;
  do {
    auto message = RPS_MESSAGE::SIGNUP;
    SendValue(rps_server, message, reply);

    for (size_t i = 0; i < 2; ++i) {
      if (actions) {
        message = actions[i];
      } else {
        if (GET_TIMER_COUNT() % 10 == 7) {  // "random" quit
          message = RPS_MESSAGE::QUIT;
        } else {
          message = static_cast<RPS_MESSAGE>(GET_TIMER_COUNT() % 3);  // "random" action
        }
      }
      SendValue(rps_server, message, reply);

      char buf[100];
      size_t len = troll::snformat(buf, "[{}, {}] ", tid, message_to_rps(message));

      switch (reply) {
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
    message = RPS_MESSAGE::QUIT;
    SendValue(rps_server, message, reply);
  loop:
    (void)0;
  } while (!actions);
}

void rps_static_test() {
  // case 1
  DEBUG_LITERAL("Case 1: Client 1 has higher priority than client 2\r\n");
  DEBUG_LITERAL("Client 1 plays rock and client 2 plays scissors, then client 1 quits\r\n");
  // but the rps_static_test() task has lower priority
  Create(PRIORITY_L4, [] { RPS_MESSAGE act[] {RPS_MESSAGE::ROCK, RPS_MESSAGE::QUIT}; rps_other_user_task(act); });
  Create(PRIORITY_L5, [] { RPS_MESSAGE act[] {RPS_MESSAGE::SCISSORS, RPS_MESSAGE::ROCK}; rps_other_user_task(act); } );

  // case 2
  DEBUG_LITERAL("Case 2: Client 1 has higher priority than client 2\r\n");
  DEBUG_LITERAL("Client 1 plays rock and client 2 quits\r\n");
  Create(PRIORITY_L4, [] { RPS_MESSAGE act[] {RPS_MESSAGE::ROCK, RPS_MESSAGE::ROCK}; rps_other_user_task(act); });
  Create(PRIORITY_L5, [] { RPS_MESSAGE act[] {RPS_MESSAGE::QUIT, RPS_MESSAGE::QUIT}; rps_other_user_task(act); } );

  // case 3
  DEBUG_LITERAL("Case 3: Client 1 and client 2 both have the same priority\r\n");
  DEBUG_LITERAL("Client 1 signs up, followed by client 2, then client 1 quits\r\n");
  Create(PRIORITY_L4, [] { RPS_MESSAGE act[] {RPS_MESSAGE::QUIT, RPS_MESSAGE::ROCK}; rps_other_user_task(act); });
  Create(PRIORITY_L4, [] { RPS_MESSAGE act[] {RPS_MESSAGE::QUIT, RPS_MESSAGE::ROCK}; rps_other_user_task(act); } );

  // case 4
  DEBUG_LITERAL("Case 4: Client 1 and client 2 both have the same priority\r\n");
  DEBUG_LITERAL("Client 1 plays rock and scissors, client 2 plays paper and scissors. They quit gracefully\r\n");
  Create(PRIORITY_L4, [] { RPS_MESSAGE act[] {RPS_MESSAGE::ROCK, RPS_MESSAGE::SCISSORS}; rps_other_user_task(act); });
  Create(PRIORITY_L4, [] { RPS_MESSAGE act[] {RPS_MESSAGE::PAPER, RPS_MESSAGE::SCISSORS}; rps_other_user_task(act); } );

  DEBUG_LITERAL("End of edge cases tests\r\n\r\n");
}

/**
 * spawns an rps server and 5 clients; let them play games on their own.
 */
void rps_first_user_task() {
  Create(PRIORITY_L1, nameserver);
  Create(PRIORITY_L1, rpsserver);
  rps_static_test();

  DEBUG_LITERAL("Starting random play using timestamp as the crappy random number.\r\n");
  DEBUG_LITERAL("Five clients are spawned, each of them will sign up, play 0~2 rounds, and re-sign up.\r\n");
  DEBUG_LITERAL("You need to press a key to continue outputs.\r\n");
  for (size_t i = 0; i < 5; ++i) {
    Create(PRIORITY_L2, [] { rps_other_user_task(nullptr); });
  }
}

static constexpr size_t PERF_REPEAT = 2048;

void perf_receiver() {
  char buffer[256];
  tid_t tid;
  while (1) {
    auto len = ReceiveValue(tid, buffer);
    ReplyValue(tid, buffer, len);
  }
  // receiver task will starve: THIS is intentional to make things simpler
}

void perf_task() {
  size_t sizes[] = { 4, 64, 256 };
  char send_buf[256], recv_buf[256];
  memset(send_buf, 0, sizeof send_buf / sizeof send_buf[0]);
  memset(recv_buf, 0, sizeof recv_buf / sizeof recv_buf[0]);

  unsigned start_tick;
  // run nocache first -> then with caches
  for (int cache_i = 0; cache_i < 4; ++cache_i) {
    char const *cache;
    switch (cache_i) {
    case 0:  // SETUP nocache
      cache = "nocache";
      break;
    case 1:  // SETUP icache
      cache = "icache";
      ICache();
      break;
    case 2:  // SETIP bcache
      cache = "bcache";
      BCache();
      break;
    case 3:  // SETUP dcache
    default:
      cache = "dcache";
      DCache();
      break;
    }
    for (int sender_first = 0; sender_first < 2; ++sender_first) {
      // in receiver first situation, the first ever receive call by the receiver is not
      // contained in the timing. this should not be a problem given PERF_REPEAT is big.
      auto target_tid = sender_first ? Create(PRIORITY_L5, perf_receiver)
        : Create(PRIORITY_L4, perf_receiver);
      char const *RS = sender_first ? "S" : "R";

      for (size_t sz_i = 0; sz_i < sizeof sizes / sizeof sizes[0]; ++sz_i) {
        auto size = sizes[sz_i];
        start_tick = GET_TIMER_COUNT();
        for (size_t i = 0; i < PERF_REPEAT; ++i) {
          SendValue(target_tid, send_buf, size, recv_buf, size);
        }
        auto ms_per = (GET_TIMER_COUNT() - start_tick) / PERF_REPEAT * NUM_TICKS_IN_1US;
        // {nocache|icache|dcache|bcache} {R|S} {4|64|256} {time}
        char buf[100];
        auto len = troll::snformat(buf, "{} {} {} {}\r\n", cache, RS, size, ms_per);
        uart_puts(0, 0, buf, len);
      }
    }
  }
}

void perf_main_task() {
  Create(PRIORITY_L4, perf_task);
}

#ifndef BENCHMARKING
#define BENCHMARKING 0
#endif

void first_user_task() {
#if BENCHMARKING
  // Reminder: after K3, we should disable IRQ during benchmarking if we want to benchmark
  perf_main_task();
#else
  rps_first_user_task();
#endif
}

}  // namespace k2

namespace k3 {

void idle_task() {
  struct time_percentage_t {
    uint32_t kernel;
    uint32_t user;
    uint32_t idle;
  };

  time_percentage_t prev_percentages = {0, 0, 0};
  time_percentage_t curr_percentages = {0, 0, 0};
  time_distribution_t td = {0, 0, 0};
  char buffer[128];
  const char* format = "\0337\033[1;80H\033[KKernel: {}%  User: {}%  Idle: {}%\r\n\0338";
  size_t len;
  size_t entries = 10;
  while (1) {
    --entries;
    // only print every 10 entries to the idle task to save the planet even more
    if (entries == 0) {
      entries = 10;
      TimeDistribution(&td);
      curr_percentages.kernel = (td.kernel_ticks * 100) / td.total_ticks;
      curr_percentages.user = ((td.total_ticks - td.kernel_ticks - td.idle_ticks) * 100) / td.total_ticks;
      curr_percentages.idle = (td.idle_ticks * 100) / td.total_ticks;

      // only print if the percentages are different
      if (curr_percentages.idle != prev_percentages.idle ||
          curr_percentages.user != prev_percentages.user || 
          curr_percentages.kernel != prev_percentages.kernel) {
        len = troll::snformat(buffer, format,
          curr_percentages.kernel,
          curr_percentages.user,
          curr_percentages.idle);
        uart_puts(0, 0, buffer, len);
        prev_percentages = curr_percentages;
      }
    }
    SaveThePlanet();
  }
}

void clock_client() {
  tid_t ptid = MyParentTid();
  tid_t tid = MyTid();

  char buffer[2];
  int replylen = SendValue(ptid, 'c', buffer);
  if (replylen != 2) {
    DEBUG_LITERAL("Something went wrong in clock client\r\n");
    return;
  }

  int interval = (int) buffer[0];
  int num_delays = (int) buffer[1];

  tid_t clock_server = WhoIs("clock_server");
  uint32_t current_time = Time(clock_server);

  const char* format = "TID: {}  Interval: {}  Delays Completed: {}  Current Tick: {}\r\n";
  char print_buffer[128];
  for (int i = 0; i < num_delays; ++i) {
    current_time = DelayUntil(clock_server, current_time + interval);
    uart_puts(0, 0, print_buffer, troll::snformat(print_buffer, format, tid, interval, i+1, current_time));
  }
}

void first_user_task() {
  Create(PRIORITY_L1, nameserver);
  Create(PRIORITY_IDLE, idle_task);
  Create(PRIORITY_L1, clockserver);
  Create(PRIORITY_L1, clocknotifier);

  // create the clients
  int clock_clients[4] = {
    Create(PRIORITY_L3, clock_client),
    Create(PRIORITY_L4, clock_client),
    Create(PRIORITY_L5, clock_client),
    Create(PRIORITY_L6, clock_client),
  };

  // the responses for each client
  // {delay_ticks, num_delays}
  char responses[4][2] = {
    {10, 20},
    {23, 9},
    {33, 6},
    {71, 3},
  };
  char msg;
  int client_tid;
  for (int i = 0; i < 4; ++i) {
    Receive(&client_tid, &msg, 1);
    if (msg == 'c') {
      for (int j = 0; j < 4; ++j) {
        if (clock_clients[j] == client_tid) {
          ReplyValue(client_tid, responses[j]);
          break;
        }
      }
    }
  }
}

} // namespace k3

namespace k4 {

void first_user_task() {
  Create(priority_t::PRIORITY_L1, nameserver);
  Create(priority_t::PRIORITY_L1, clockserver);
  Create(priority_t::PRIORITY_L1, clocknotifier);
  gtkterm::init_tasks();
  merklin::init_tasks();
  trains::init_tasks();
  ui::init_tasks();
}

} // namespace k4
