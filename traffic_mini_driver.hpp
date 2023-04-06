#pragma once

#include "generic/utils.hpp"
#include "traffic.hpp"

namespace traffic {

  using train_courier_t = utils::courier_runner<
    traffic_msg_header::TO_TC_COURIER,
    sizeof(utils::enumed_class<tcmd::tc_msg_header, tcmd::speed_cmd>)
  >;
  using switch_courier_t = utils::courier_runner<traffic_msg_header::TO_SWITCH_COURIER, sizeof(switch_cmd)>;

  /**
   * simulates the driver of one train.
   */
  struct mini_driver {
    /**
     * dependencies
     */
    internal_train_state *train;  // observer
    tracks::blocked_track_nodes_t *reserved_nodes;  // observer
    train_courier_t *train_courier; // observer
    switch_courier_t *switch_courier; // observer

    /**
     * path currently being followed.
     */
    etl::optional<tracks::node_path_reversal_ok_t> path {etl::nullopt};
    /**
     * current node in path.
     */
    size_t i {}, j {};
    /**
     * final destination. {} means not set.
     */
    position_t dest {};
    /**
     * distance required for braking.
     */
    int braking_dist {};
    /**
     * wait timer before reacceleration after reversing.
     */
    int reverse_timer {};

    enum state_t {
      // noop
      THINKING = 0,
      // when starting a new path segment, do acceleration
      ENROUTE_ACCEL_TO_START_SEGMENT,
      ENROUTE_RUNNING_SEGMENT,
      ENROUTE_BREAKING,
      ENROUTE_END_SEGMENT,
      ENROUTE_WAITING_TO_REVERSE,
      // if we need to go to sensor but end up not being at the sensor, reaccelerate
      // to fix it.
      FIX_RUNNING_UP,
      FIX_WAITING_TO_REVERSE_1,
      FIX_RUNNING_BACK,
      FIX_WAITING_TO_REVERSE_2,
    } state {};

    void reset() {
      path = etl::nullopt;
      i = j = 0;
      dest = {};
      braking_dist = 0;
      state = THINKING;
    }

    auto const &segment() const {
      return std::get<0>((*path)[i]);
    }

    /**
     * predict() will set this for us..
     */
    int &segment_dist_left() {
      return std::get<1>((*path)[i]);
    }

    void set_speed(int speed);

    void set_switch(int sw, bool is_straight);

    void emergency_stop() {
      dest = {};
      path = etl::nullopt;
      state = THINKING;
      set_speed(16);
    }

    void switch_ahead_on_demand();

    /**
     * do an action and advances state.
     */
    void perform();
  };
}  // namespace traffic
