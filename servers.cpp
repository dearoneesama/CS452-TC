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
  etl::queue<tid_t, MAX_QUEUED_PLAYERS> queue;
  struct {
    tid_t tid = 0;
    RPS_MESSAGE which = RPS_MESSAGE::SIGNUP;
  } p[2];
  bool match = false;

  tid_t request_tid;
  char message;
  while (1) {
    // get matched players from queue first
    if (!match && queue.size() >= 2) {
      p[0].tid = queue.front();
      queue.pop();
      p[1].tid = queue.front();
      queue.pop();
      rps_reply(p[0].tid, RPS_REPLY::SEND_ACTION);
      rps_reply(p[1].tid, RPS_REPLY::SEND_ACTION);
      match = true;
      continue;
    }

    if (Receive(reinterpret_cast<int *>(&request_tid), &message, sizeof message) < 1) {
      continue;
    }
    switch (auto act = static_cast<RPS_MESSAGE>(message)) {
    case RPS_MESSAGE::SIGNUP: {
      // ignore double signup after a match
      if (match && (request_tid == p[0].tid || request_tid == p[1].tid)) {
        rps_reply(request_tid, RPS_REPLY::INVALID);
      } else {
        queue.push(request_tid);  // client is blocked
      }
      break;
    }

    case RPS_MESSAGE::PAPER:
    case RPS_MESSAGE::ROCK:
    case RPS_MESSAGE::SCISSORS: {
      if (match && request_tid == p[0].tid) {
        p[0].which = act;  // client is blocked
      } else if (match && request_tid == p[1].tid) {
        p[1].which = act;  // client is blocked
      } else {
        // likely the client is sending action after the session is dead
        // (other play quits). in this case send abandon too.
        // technically it gets mixed up if another client sends play without signing
        // up and this if cond is hit. but this reply is ok too and that's user error
        rps_reply(request_tid, RPS_REPLY::ABANDONED);
      }
      break;
    }

    case RPS_MESSAGE::QUIT: {
      if (match && (request_tid == p[0].tid || request_tid == p[1].tid)) {
        rps_reply(request_tid, RPS_REPLY::ABANDONED);
        // if other player already gave action, then reply with abandon right now;
        // otherwise if the current task is the first one to give action (quit)
        // then the other task can give action|quit later, and receive an abandon
        if (request_tid == p[0].tid && p[1].which != RPS_MESSAGE::SIGNUP) {
          rps_reply(p[1].tid, RPS_REPLY::ABANDONED);
        } else if (request_tid == p[1].tid && p[0].which != RPS_MESSAGE::SIGNUP) {
          rps_reply(p[0].tid, RPS_REPLY::ABANDONED);
        }
        match = false;
        p[0] = p[1] = {};
      } else {
        rps_reply(request_tid, RPS_REPLY::ABANDONED);
      }
      break;
    }
    }

    if (match && p[0].which != RPS_MESSAGE::SIGNUP && p[1].which != RPS_MESSAGE::SIGNUP) {
      // send result, and continue asking for play|quit
      auto cmp = rps_who_wins(p[0].which, p[1].which);
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
      rps_reply(p[0].tid, reply0);
      rps_reply(p[1].tid, reply1);
      p[0].which = p[1].which = RPS_MESSAGE::SIGNUP;
    }
  }
}
