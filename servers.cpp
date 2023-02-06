#include <tuple>
#include <etl/queue.h>
#include "containers.hpp"
#include "servers.hpp"
#include "kstddefs.hpp"
#include "user_syscall.h"

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

void rpcserver() {
  RegisterAs("rpcserver");
  etl::queue<tid_t, MAX_QUEUED_PLAYERS> queue;
  struct {
    tid_t tid;
    RPC_MESSAGE which;
  } p[2];

  enum class state_t {
    WAITING_FOR_BOTH,
    WAITING_FOR_OTHER,
    WAITING_FOR_BOTH_ACT,
    WAITING_FOR_OTHER_ACT,
    RESULT,
  } state = state_t::WAITING_FOR_BOTH;

  tid_t request_tid;
  char message;
  char reply;

  while (1) {
    switch (state) {
      case state_t::WAITING_FOR_BOTH: {
        if (queue.size()) {
          p[0].tid = queue.front();
          queue.pop();
          state = state_t::WAITING_FOR_OTHER;
          break;
        }
        Receive((int*)&request_tid, &message, sizeof message);
        if (message == static_cast<char>(RPC_MESSAGE::SIGNUP)) {
          p[0].tid = request_tid;
          state = state_t::WAITING_FOR_OTHER;
        } else if (message == static_cast<char>(RPC_MESSAGE::QUIT)) {
          // noop <= extra calling from a task after one task calls quit and this leaves
          // WAITING_FOR_BOTH_ACT state.
          // same handling is applied to other states.
          reply = static_cast<char>(RPC_REPLY::ABANDONED);
          Reply(request_tid, &reply, 1);
        }
        break;
      }

      case state_t::WAITING_FOR_OTHER: {
        reply = static_cast<char>(RPC_REPLY::SEND_ACTION);
        if (queue.size()) {
          p[1].tid = queue.front();
          queue.pop();
          Reply(p[0].tid, &reply, 1);
          Reply(p[1].tid, &reply, 1);
          state = state_t::WAITING_FOR_BOTH_ACT;
          break;
        }
        Receive((int*)&request_tid, &message, sizeof message);
        if (message == static_cast<char>(RPC_MESSAGE::SIGNUP)) {
          p[1].tid = request_tid;
          Reply(p[0].tid, &reply, 1);
          Reply(p[1].tid, &reply, 1);
          state = state_t::WAITING_FOR_BOTH_ACT;
        } else if (message == static_cast<char>(RPC_MESSAGE::QUIT)) {
          reply = static_cast<char>(RPC_REPLY::ABANDONED);
          Reply(request_tid, &reply, 1);
        }
        break;
      }

      case state_t::WAITING_FOR_BOTH_ACT: {
        Receive((int*)&request_tid, &message, sizeof message);
        switch (static_cast<RPC_MESSAGE>(message)) {
        case RPC_MESSAGE::SIGNUP:
          queue.push(request_tid);
          break;
        case RPC_MESSAGE::ROCK:
        case RPC_MESSAGE::PAPER:
        case RPC_MESSAGE::SCISSORS:
          if (request_tid == p[0].tid) {
            p[0].which = static_cast<RPC_MESSAGE>(message);
            state = state_t::WAITING_FOR_OTHER_ACT;
          } else if (request_tid == p[1].tid) {
            std::tie(p[0].tid, p[1].tid) = std::make_tuple(p[1].tid, p[0].tid);
            p[0].which = static_cast<RPC_MESSAGE>(message);
            state = state_t::WAITING_FOR_OTHER_ACT;
          }
          break;
        case RPC_MESSAGE::QUIT:
          // match is abandoned => quit both tasks including the one
          // that did not call quit
          reply = static_cast<char>(RPC_REPLY::ABANDONED);
          if (request_tid == p[0].tid || request_tid == p[1].tid) {
            Reply(p[0].tid, &reply, 1);
            Reply(p[1].tid, &reply, 1);
            state = state_t::WAITING_FOR_BOTH;
          } else {
            Reply(request_tid, &reply, 1);
          }
          break;
        default:
          break;
        }
        break;
      }

      case state_t::WAITING_FOR_OTHER_ACT: {
        Receive((int*)&request_tid, &message, sizeof message);
        switch (static_cast<RPC_MESSAGE>(message)) {
        case RPC_MESSAGE::SIGNUP:
          queue.push(request_tid);
          break;
        case RPC_MESSAGE::ROCK:
        case RPC_MESSAGE::PAPER:
        case RPC_MESSAGE::SCISSORS:
          if (request_tid == p[1].tid) {
            p[1].which = static_cast<RPC_MESSAGE>(message);
            state = state_t::RESULT;
          }
          break;
        case RPC_MESSAGE::QUIT: {
          reply = static_cast<char>(RPC_REPLY::ABANDONED);
          if (request_tid == p[0].tid || request_tid == p[1].tid) {
            Reply(p[0].tid, &reply, 1);
            Reply(p[1].tid, &reply, 1);
            state = state_t::WAITING_FOR_BOTH;
          } else {
            Reply(request_tid, &reply, 1);
          }
        }
          break;
        default:
          break;
        }
        break;
      }

      case state_t::RESULT: {
        char reply1;
        if ((p[0].which == RPC_MESSAGE::PAPER && p[1].which == RPC_MESSAGE::ROCK)
          || (p[0].which == RPC_MESSAGE::ROCK && p[1].which == RPC_MESSAGE::SCISSORS)
          || (p[0].which == RPC_MESSAGE::SCISSORS && p[1].which == RPC_MESSAGE::PAPER)
        ) {
          reply = static_cast<char>(RPC_REPLY::WIN_AND_SEND_ACTION);
          reply1 = static_cast<char>(RPC_REPLY::LOSE_AND_SEND_ACTION);
        } else if ((p[0].which == RPC_MESSAGE::PAPER && p[1].which == RPC_MESSAGE::SCISSORS)
          || (p[0].which == RPC_MESSAGE::ROCK && p[1].which == RPC_MESSAGE::PAPER)
          || (p[0].which == RPC_MESSAGE::SCISSORS && p[1].which == RPC_MESSAGE::ROCK)
        ) {
          reply = static_cast<char>(RPC_REPLY::LOSE_AND_SEND_ACTION);
          reply1 = static_cast<char>(RPC_REPLY::WIN_AND_SEND_ACTION);
        } else {
          reply = reply1 = static_cast<char>(RPC_REPLY::TIE_AND_SEND_ACTION);
        }
        Reply(p[0].tid, &reply, 1);
        Reply(p[1].tid, &reply1, 1);
        state = state_t::WAITING_FOR_BOTH_ACT;
        break;
      }
    }
  }
}
