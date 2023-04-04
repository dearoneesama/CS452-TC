#pragma once

#include <cstdint>

// old name: trains
namespace tcmd {

enum class tc_reply : char {
  OK = 0,
  TRAIN_ALREADY_REVERSING,
  SWITCH_UNCHANGED,
};

enum class tc_msg_header : uint64_t {
  SPEED = 'd',

  REVERSE_CMD_PART_1 = 'r', // makes the train stop
  REVERSE_CMD_PART_2 = 'v', // reverse, not accel
	REVERSE_CMD_PART_3 = 'a', // reaccel

  SWITCH_CMD_PART_1 = 's', // change direction
  SWITCH_CMD_PART_2 = 'w', // turn off

  SENSOR_CMD = 'n',

  GO_CMD = 'g',

  SET_RESET_MODE = 'm',
};

struct reverse_cmd {
  int train;
};

enum class switch_dir_t : char {
  S = 0x21,
  C = 0x22,
  NONE = 0,
};

enum class special_cmd : unsigned char {
  TURNOFF_SWITCH = 0x20,
  RESET_MODE = 0xC0,
  READ_SENSORS = 128 + 5,
  GO = 96,
};

struct switch_cmd {
  int switch_num;
  switch_dir_t switch_dir;
};

struct speed_cmd {
  int train;
  int speed;
};

const char * const TRAIN_TASK_NAME = "traintt";
const char * const REVERSE_TASK_NAME = "reverset";
const char * const SWITCH_TASK_NAME = "switchtt";
const char * const SENSOR_TASK_NAME = "sensortt";

void init_tasks();

}
