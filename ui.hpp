#pragma once

#include <fpm/fixed.hpp>
#include "kern/kstddefs.hpp"
#include "kern/user_syscall_typed.hpp"
#include "generic/format.hpp"
#include "generic/utils.hpp"
#include "tracks.hpp"

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
  TRAIN_READ,
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

using sensor_read = tracks::sensor_read;
using switch_read = tracks::switch_cmd;

struct train_read {
  int num, cmd;
  tracks::position_t dest, pos;
  fpm::fixed_24_8 speed, delta_t, delta_d;
};

template<class T>
inline etl::string<11> stringify_pos(T const &pos) {
  auto name = static_cast<etl::string_view>(pos.name);
  if (!name.size()) {
    return "?";
  } else if (pos.offset == 0) {
    return pos.name;
  } else if (pos.offset < 0) {
    return troll::sformat<11>("{}{}", name, pos.offset);
  } else {
    return troll::sformat<11>("{}+{}", name, pos.offset);
  }
}

const char * const DISPLAY_CONTROLLER_NAME = "displayc";

void init_tasks();

struct ui_sender {
  template<class T>
  int send_value(T &&data, size_t len = sizeof(T)) {
    return SendValue(display_controller(), std::forward<T>(data), len, null_reply);
  }

  template<size_t N>
  int send_notice(etl::string<N> const &str) {
    utils::enumed_class<display_msg_header, char[N]> message;
    message.header = display_msg_header::USER_NOTICE;
    auto len = troll::snformat(message.data, sizeof message.data, str.c_str());
    return send_value(message, sizeof message.header + len + 1);
  }

  template<size_t N>
  int send_notice(char const (&str)[N]) {
    utils::enumed_class<display_msg_header, char[N]> message;
    message.header = display_msg_header::USER_NOTICE;
    auto len = troll::snformat(message.data, sizeof message.data, str);
    return send_value(message, sizeof message.header + len + 1);
  }

  decltype(TaskFinder("")) display_controller { TaskFinder(DISPLAY_CONTROLLER_NAME) };
};

/**
 * global helper structure for sending messages to the display controller task.
 */
ui_sender &out();

}
