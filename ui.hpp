#pragma once

#include "kstddefs.hpp"
#include "user_syscall_typed.hpp"
#include "format.hpp"
#include "utils.hpp"

namespace ui {

enum class display_msg_header : uint64_t {
  IDLE_MSG = 'i',
  SENSOR_MSG = 'n',
  USER_INPUT = 'u',
  USER_ENTER = 'e',
  USER_BACKSPACE = 'b',
  USER_NOTICE = 's',
  TIMER_CLOCK_MSG = 't',
  SWITCHES = 'w',
};

struct timer_clock_t {
  int hours;
  int minutes;
  int seconds;
  int hundred_ms;
};

struct time_percentage_t {
  uint32_t kernel;
  uint32_t user;
  uint32_t idle;
};

const char * const DISPLAY_CONTROLLER_NAME = "displayc";

void init_tasks();

template <size_t N>
inline void send_notice(char const (&msg)[N]) {
  auto display_controller = TaskFinder(DISPLAY_CONTROLLER_NAME);
  utils::enumed_class<display_msg_header, char[N]> message;
  message.header = display_msg_header::USER_NOTICE;
  troll::snformat(message.data, sizeof message.data, msg);
  SendValue(display_controller(), message, null_reply);
}

}
