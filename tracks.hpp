#pragma once

#include <etl/string.h>
#include "trains.hpp"

namespace tracks {

  void init_tasks();

  static const char * const TRACK_SERVER_TASK_NAME = "trackctl";

  enum class track_reply_msg : char {
    OK = 'o',
  };

  enum class track_msg_header : uint64_t {
    TRAIN_SPEED_CMD,
    SWITCH_CMD,
    SENSOR_READ,
    TRAIN_PREDICT,
    TRAIN_POS_INIT,
  };

  using speed_cmd = trains::speed_cmd;
  using switch_cmd = trains::switch_cmd;
  using switch_dir_t = trains::switch_dir_t;

  struct sensor_read {
    etl::string<3> sensor;
    unsigned tick;
  };

  struct position_t {
    etl::string<4> name {};
    int offset {};
  };

  struct train_pos_init_msg {
    int train;
    etl::string<4> name;
  };
}  // namespace tracks
