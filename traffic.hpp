#pragma once

#include <etl/string.h>
#include "tcmd.hpp"
#include "generic/utils.hpp"

namespace traffic {

  void init_tasks();

  static const char * const TRAFFIC_SERVER_TASK_NAME = "trafficctl";

  enum class traffic_reply_msg : char {
    OK = 'o',
  };

  enum class traffic_msg_header : uint64_t {
    TRAIN_SPEED_CMD,
    SWITCH_CMD,
    SENSOR_READ,
    TRAIN_PREDICT,
    TRAIN_POS_INIT,
    TRAIN_POS_DEINIT,
    TRAIN_POS_GOTO,
    TRAINS_STOP,
    TO_TC_COURIER,
    TO_SWITCH_COURIER,
  };

  using speed_cmd = tcmd::speed_cmd;
  using switch_cmd = tcmd::switch_cmd;
  using switch_dir_t = tcmd::switch_dir_t;

  struct sensor_read {
    utils::sd_buffer<4> sensor;
    unsigned tick;
  };

  struct position_t {
    utils::sd_buffer<5> name;
    int offset {};
  };

  struct train_pos_init_msg {
    int train;
    utils::sd_buffer<5> name;
    int offset;
  };

  using train_pos_goto_msg = train_pos_init_msg;
  using train_deinit_msg = int;
}  // namespace traffic
