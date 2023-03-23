#pragma once

#include <etl/array.h>
#include "containers.hpp"
#include "track_new.hpp"
#include "trains.hpp"

namespace tracks {
  static constexpr size_t num_switches = 22;
  static constexpr size_t num_trains = 6;

  /**
   * a static array of switch names.
   */
  etl::array<int, num_switches> const &valid_switches();

  /**
   * a static array of train numbers.
   */
  etl::array<int, num_trains> const &valid_trains();

  /**
   * a static adjacency list of track nodes.
   */
  troll::string_map<const track_node *, 4, TRACK_MAX> const &valid_nodes();

  void init_tasks();

  static const char * const TRACK_SERVER_TASK_NAME = "trackctl";

  enum class track_reply_msg : char {
    OK = 'o',
  };

  enum class track_msg_header : uint64_t {
    TRAIN_SPEED_CMD,
    SWITCH_CMD,
    SENSOR_READ,
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
}  // namespace tracks
