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
