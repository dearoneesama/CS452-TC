#include <etl/queue.h>
#include "containers.hpp"
#include "servers.hpp"
#include "kstddefs.hpp"
#include "utils.hpp"
#include "user_syscall.h"
#include "rpi.hpp"
#include "notifiers.hpp"

void nameserver() {
  troll::string_map<tid_t, MAX_TASK_NAME_LENGTH, MAX_NUM_TASKS> lookup;

  char tid_buffer[4];

  tid_t request_tid;
  char buffer[MAX_TASK_NAME_LENGTH + 1];
  while (1) {
    // received string is guaranteed to be null-terminated if client
    // uses Register() or WhoIs()
    int request = Receive((int*)&request_tid, buffer, sizeof buffer);
    if (request > 0) {
      switch (buffer[0]) {
      case 'r': { // register
        lookup[buffer + 1] = request_tid;
        Reply(request_tid, "1", 1);
        break;
      }
      case 'w': { // who is
        if (auto it = lookup.find(buffer + 1); it != lookup.cend()) {
          auto target_tid = it->second;
          tid_buffer[0] = target_tid;
          tid_buffer[1] = target_tid >> 8;
          tid_buffer[2] = target_tid >> 16;
          tid_buffer[3] = target_tid >> 24;
          Reply(request_tid, tid_buffer, 4);
        } else {
          Reply(request_tid, 0, 0); // no such name
        }
        break;
      }
      default:
        break;
      }
    }
  }
}

namespace {
  // 0: tie, 1: m1 wins, -1: m2 wins
  int rps_who_wins(RPS_MESSAGE m1, RPS_MESSAGE m2) {
    if (m1 == m2) {
      return 0;
    } else if (
      (m1 == RPS_MESSAGE::ROCK     && m2 == RPS_MESSAGE::PAPER) ||
      (m1 == RPS_MESSAGE::PAPER    && m2 == RPS_MESSAGE::SCISSORS) ||
      (m1 == RPS_MESSAGE::SCISSORS && m2 == RPS_MESSAGE::ROCK)
    ) {
      return -1;
    }
    return 1;
  }

  void rps_reply(tid_t tid, RPS_REPLY r) {
    auto buf = static_cast<char>(r);
    Reply(tid, &buf, sizeof buf);
  }
}  // namespace


void rpsserver() {
  RegisterAs("rpsserver");
  etl::queue<tid_t, 2> queue;

  struct match {
    tid_t opponent = 0;
    RPS_MESSAGE which = RPS_MESSAGE::SIGNUP;
  };
  // tid -> match obj -> tid
  etl::unordered_map<tid_t, match, MAX_RPS_PLAYERS * 2> matches;

  tid_t request_tid;
  char message;
  while (1) {
    // get matched players from queue first
    if (queue.size() >= 2) {
      tid_t p0, p1;
      p0 = queue.front();
      queue.pop();
      p1 = queue.front();
      queue.pop();
      matches[p0] = {p1, RPS_MESSAGE::SIGNUP};
      matches[p1] = {p0, RPS_MESSAGE::SIGNUP};
      rps_reply(p0, RPS_REPLY::SEND_ACTION);
      rps_reply(p1, RPS_REPLY::SEND_ACTION);
      continue;
    }

    if (Receive(reinterpret_cast<int *>(&request_tid), &message, sizeof message) < 1) {
      continue;
    }
    switch (auto act = static_cast<RPS_MESSAGE>(message)) {
    case RPS_MESSAGE::SIGNUP: {
      // ignore double signup after a match
      if (matches.find(request_tid) != matches.cend()) {
        rps_reply(request_tid, RPS_REPLY::INVALID);
      } else {
        queue.push(request_tid);  // client is blocked
      }
      break;
    }

    case RPS_MESSAGE::PAPER:
    case RPS_MESSAGE::ROCK:
    case RPS_MESSAGE::SCISSORS: {
      if (auto found = matches.find(request_tid); found == matches.cend()) {
        // likely the client is sending action after the session is dead
        // (other play quits). in this case send abandon too.
        // technically it gets mixed up if another client sends play without signing
        // up and this if cond is hit. but this reply is ok too and that's user error
        rps_reply(request_tid, RPS_REPLY::ABANDONED);
      } else {
        found->second.which = act;  // client is blocked
      }
      break;
    }

    case RPS_MESSAGE::QUIT: {
      if (auto found = matches.find(request_tid); found == matches.cend()) {
        // other opponent quit already or request is troll
        rps_reply(request_tid, RPS_REPLY::ABANDONED);
      } else {
        auto opponent_tid = found->second.opponent;
        rps_reply(request_tid, RPS_REPLY::ABANDONED);
        if (auto opponent = matches[opponent_tid]; opponent.which != RPS_MESSAGE::SIGNUP) {
          // other play has sent a command already. reply to it as well.
          rps_reply(opponent_tid, RPS_REPLY::ABANDONED);
        }
        matches.erase(request_tid);
        matches.erase(opponent_tid);
      }
      break;
    }
    }

    for (auto &&[tid, match] : matches) {
      auto &opponent = matches[match.opponent];
      if (match.which == RPS_MESSAGE::SIGNUP || opponent.which == RPS_MESSAGE::SIGNUP) {
        continue;
      }
      // send result, and continue asking for play|quit
      auto cmp = rps_who_wins(match.which, opponent.which);
      RPS_REPLY reply0, reply1;
      if (cmp == -1) {
        reply0 = RPS_REPLY::LOSE_AND_SEND_ACTION;
        reply1 = RPS_REPLY::WIN_AND_SEND_ACTION;
      } else if (cmp == 1) {
        reply0 = RPS_REPLY::WIN_AND_SEND_ACTION;
        reply1 = RPS_REPLY::LOSE_AND_SEND_ACTION;
      } else {
        reply0 = reply1 = RPS_REPLY::TIE_AND_SEND_ACTION;
      }
      rps_reply(tid, reply0);
      rps_reply(match.opponent, reply1);
      match.which = opponent.which = RPS_MESSAGE::SIGNUP;
    }
  }
}

void clockserver() {
  if (RegisterAs("clock_server") != 0) {
    // something went wrong in registration
    return;
  }

  uint32_t ticks_in_10ms = 0;
  // tid -> target tick
  etl::unordered_map<tid_t, uint32_t, MAX_NUM_TASKS> delays;

  constexpr char buffer_length = 5;
  char buffer[buffer_length];

  tid_t request_tid;
  while (1) {
    int request = Receive((int*)&request_tid, buffer, buffer_length);
    if (request <= 0) continue;
    switch (static_cast<CLOCK_MESSAGE>(buffer[0])) {
      case CLOCK_MESSAGE::TIME: {
        buffer[0] = static_cast<char>(CLOCK_REPLY::TIME_OK);
        utils::uint32_to_buffer(buffer + 1, ticks_in_10ms);
        Reply(request_tid, buffer, 5);
        break;
      }
      case CLOCK_MESSAGE::DELAY: {
        uint32_t delay = utils::buffer_to_uint32(buffer + 1);
        if (delay > 0) {
          delays[request_tid] = delay;
        } else {
          buffer[0] = static_cast<char>(CLOCK_REPLY::DELAY_OK);
          utils::uint32_to_buffer(buffer + 1, ticks_in_10ms);
          Reply(request_tid, buffer, 5);
        }
        break;
      }
      case CLOCK_MESSAGE::DELAY_UNTIL: {
        uint32_t delay_until = utils::buffer_to_uint32(buffer + 1);
        int delay = delay_until - ticks_in_10ms;
        if (delay > 0) {
          delays[request_tid] = delay;
        } else if (delay == 0) {
          buffer[0] = static_cast<char>(CLOCK_REPLY::DELAY_OK);
          utils::uint32_to_buffer(buffer + 1, ticks_in_10ms);
          Reply(request_tid, buffer, 5);
        } else {
          buffer[0] = static_cast<char>(CLOCK_REPLY::DELAY_NEGATIVE);
          Reply(request_tid, buffer, 1);
        }
        break;
      }
      case CLOCK_MESSAGE::NOTIFY: {
        ++ticks_in_10ms;
        buffer[0] = static_cast<char>(CLOCK_REPLY::NOTIFY_OK);
        Reply(request_tid, buffer, 1);

        buffer[0] = static_cast<char>(CLOCK_REPLY::DELAY_OK);
        utils::uint32_to_buffer(buffer + 1, ticks_in_10ms);

        // instead of removing them from the map, simply assume that
        // delay == 0 means a null state
        for (auto &&[tid, delay] : delays) {
          if (delay == 0) {
            continue;
          }
          delays[tid] -= 1;
          if (delays[tid] == 0) {
            Reply(tid, buffer, 5);
          }
        }
        break;
      }
      default: break;
    }
  }
}

void tx_uartserver(char uart_channel) {
  if (uart_channel == 0) {
    RegisterAs("txuarts0");
  } else {
    RegisterAs("txuarts1");
  }

  tid_t notifier = Create(priority_t::PRIORITY_L5, tx_uartnotifier);
  tid_t request_tid;
  char msg;
  Receive((int*)&request_tid, &msg, 1);
  char config[2];
  config[0] = uart_channel == 0 ? 0 : 1;
  config[1] = uart_channel;
  Reply(notifier, config, 2);

  char buffer[2];
  etl::queue<tid_t, 50> requester_queue;
  etl::queue<char, 50> char_queue;
  const char* reply = "o";

  while (1) {
    int request = Receive((int*)&request_tid, buffer, 2);
    if (request <= 0) continue;

    switch (buffer[0]) {
      case 'n': {
        // notifier
        tid_t requester;
        char c;
        while (!requester_queue.empty() && is_uart_writable(0, uart_channel)) {
          requester = requester_queue.front();
          requester_queue.pop();
          c = char_queue.front();
          char_queue.pop();
          uart_write(0, uart_channel, c);
          Reply(requester, reply, 1);
        }
        break;
      }
      case 'p': {
        // putc
        requester_queue.push(request_tid);
        char_queue.push(buffer[1]);
        tid_t requester;
        char c;
        while (!requester_queue.empty() && is_uart_writable(0, uart_channel)) {
          requester = requester_queue.front();
          requester_queue.pop();
          c = char_queue.front();
          char_queue.pop();
          uart_write(0, uart_channel, c);
          Reply(requester, reply, 1);
        }

        if (!requester_queue.empty()) {
          Reply(notifier, reply, 1);
        }
        break;
      }
      default: {
        break;
      }
    }
  }
}

void rx_uartserver(char uart_channel) {
  if (uart_channel == 0) {
    RegisterAs("rxuarts0");
  } else {
    RegisterAs("rxuarts1");
  }

  tid_t notifier = Create(priority_t::PRIORITY_L5, rx_uartnotifier);
  tid_t request_tid;
  char msg;
  Receive((int*)&request_tid, &msg, 1);
  char config = uart_channel;
  Reply(notifier, &config, 1);

  etl::queue<tid_t, 50> requester_queue;
  etl::queue<char, 512> char_queue;

  char buffer;
  const char *reply = "o";
  while (1) {
    int request = Receive((int*)&request_tid, &buffer, 1);
    if (request <= 0) continue;

    switch (buffer) {
      case 'n':
        // notifier
        while (is_uart_readable(0, uart_channel)) {
          char_queue.push(uart_read(0, uart_channel));
          if (!requester_queue.empty()) {
            char c = char_queue.front();
            char_queue.pop();
            tid_t requester = requester_queue.front();
            requester_queue.pop();
            Reply(requester, &c, 1);
          }
        }
        break;
      case 'g': {
        // getc
        requester_queue.push(request_tid);
        while (is_uart_readable(0, uart_channel)) {
          char_queue.push(uart_read(0, uart_channel));
          if (!requester_queue.empty()) {
            char c = char_queue.front();
            char_queue.pop();
            tid_t requester = requester_queue.front();
            requester_queue.pop();
            Reply(requester, &c, 1);
          }
        }
        if (!requester_queue.empty()) {
          Reply(notifier, reply, 1);
        }
        break;
      }
      default:
        break;
      }
  }
}
