#pragma once

#include <etl/string.h>
#include "tcmd.hpp"
#include "generic/utils.hpp"
#include "track_consts.hpp"
#include "track_graph.hpp"

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

  // DETAIL

  /**
   * how many ticks to wait before assuming a train has left a sensor.
   */
  static constexpr auto train_sensor_expire_timeout = 50;

  static constexpr auto predict_react_interval = 15;  // 150 ms

  /**
   * information about a train.
   * 
   * units for distances are mm.
   * units for times are second.
   */
  struct internal_train_state {
    // train number
    int num {};
    // last speed command given to it
    int cmd {};
    // low speed to high speed?
    bool lo_to_hi {};

    struct snapshot_t {
      position_t pos {};
      // positive
      tracks::fp speed {};
      // positive for acceleration, negative for deceleration
      tracks::fp accel {};
      unsigned tick {};
    };

    // information based on last sensor reading
    snapshot_t sensor_snap {};
    // sensor snap error terms
    tracks::fp sn_delta_t {}, sn_delta_d {};
    // train ever received speed change command since last sensor?
    bool sn_accelerated {};
    // average speed across sensor
    troll::pseudo_moving_average<tracks::fp> sn_avg_speed {};

    // information based on last tick
    // the two fields will be in sync once train hits a sensor
    snapshot_t tick_snap {};

    // train is approaching source sensor instead of leaving?
    bool approaching_sensor {};
    // train is on the sensor thus spamming lots of sensor reads?
    bool sitting_on_sensor {};
  };
}  // namespace traffic
