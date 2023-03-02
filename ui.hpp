#pragma once

#include "kstddefs.hpp"

namespace ui {

enum display_msg_header {
  IDLE_MSG = 'i',
  SENSOR_MSG = 'n',
  USER_INPUT = 'u',
  STRING = 's',
  TIMER_CLOCK_MSG = 't',
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

}
