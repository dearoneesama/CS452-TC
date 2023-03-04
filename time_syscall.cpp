#include "user_syscall_typed.hpp"
#include "utils.hpp"
#include "rpi.hpp"
#include "servers.hpp"

int Time(int tid) {
  char buffer[5];
  int replylen = SendValue(tid, CLOCK_MESSAGE::TIME, buffer);
  if (replylen != 5 || buffer[0] != static_cast<char>(CLOCK_REPLY::TIME_OK)) {
    return -1;
  }
  return utils::buffer_to_uint32(buffer + 1);
}

int Delay(int tid, int ticks) {
  if (ticks < 0) {
    return -2;
  }
  char buffer[5];
  buffer[0] = static_cast<char>(CLOCK_MESSAGE::DELAY);
  utils::uint32_to_buffer(buffer + 1, ticks);
  int replylen = SendValue(tid, buffer, buffer);
  if (replylen != 5 || buffer[0] != static_cast<char>(CLOCK_REPLY::DELAY_OK)) {
    return -1;
  }
  return utils::buffer_to_uint32(buffer + 1);
}

int DelayUntil(int tid, int ticks) {
  char buffer[5];
  buffer[0] = static_cast<char>(CLOCK_MESSAGE::DELAY_UNTIL);
  utils::uint32_to_buffer(buffer + 1, ticks);
  int replylen = SendValue(tid, buffer, buffer);
  if (replylen > 0 && buffer[0] == static_cast<char>(CLOCK_REPLY::DELAY_NEGATIVE)) {
    return -2; // negative delay
  }
  if (replylen != 5 || buffer[0] != static_cast<char>(CLOCK_REPLY::DELAY_OK)) {
    return -1;
  }
  return utils::buffer_to_uint32(buffer + 1);
}
