#include "user_syscall_typed.hpp"
#include "../utils.hpp"
#include "rpi.hpp"
#include "servers.hpp"

int Time(int tid) {
  utils::enumed_class<CLOCK_REPLY, uint32_t> reply;
  int replylen = SendValue(tid, CLOCK_MESSAGE::TIME, reply);
  if (replylen < 1 || reply.header != CLOCK_REPLY::TIME_OK) {
    return -1;
  }
  return reply.data;
}

int Delay(int tid, int ticks) {
  if (ticks < 0) {
    return -2;
  }
  utils::enumed_class<CLOCK_REPLY, uint32_t> reply;
  int replylen = SendValue(tid, utils::enumed_class {
    CLOCK_MESSAGE::DELAY,
    static_cast<uint32_t>(ticks),
  }, reply);
  if (replylen < 1 || reply.header != CLOCK_REPLY::DELAY_OK) {
    return -1;
  }
  return reply.data;
}

int DelayUntil(int tid, int ticks) {
  utils::enumed_class<CLOCK_REPLY, uint32_t> reply;
  int replylen = SendValue(tid, utils::enumed_class {
    CLOCK_MESSAGE::DELAY_UNTIL,
    static_cast<uint32_t>(ticks),
  }, reply);
  if (replylen > 0 && reply.header == CLOCK_REPLY::DELAY_NEGATIVE) {
    return -2; // negative delay
  }
  if (replylen < 1 || reply.header != CLOCK_REPLY::DELAY_OK) {
    return -1;
  }
  return reply.data;
}
