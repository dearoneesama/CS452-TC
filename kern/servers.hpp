#pragma once

#include <cstddef>

// length includes nul
static constexpr size_t MAX_TASK_NAME_LENGTH = 21;

void nameserver();

static constexpr size_t MAX_RPS_PLAYERS = 20;

enum class RPS_MESSAGE : char {
  ROCK = 0,
  PAPER,
  SCISSORS,
  SIGNUP,
  QUIT,
};

enum class RPS_REPLY : char {
  SEND_ACTION,
  WIN_AND_SEND_ACTION,
  LOSE_AND_SEND_ACTION,
  TIE_AND_SEND_ACTION,
  ABANDONED,
  INVALID,
};

void rpsserver();

enum class CLOCK_MESSAGE : char {
  NOTIFY = 0,
  TIME,
  DELAY,
  DELAY_UNTIL,
};

enum class CLOCK_REPLY : char {
  NOTIFY_OK = 0,
  TIME_OK,
  DELAY_OK,
  DELAY_NEGATIVE,
};

void clockserver();