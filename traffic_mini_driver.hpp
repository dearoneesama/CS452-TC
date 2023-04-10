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
    internal_switch_state *switches;  // observer
    tracks::blocked_track_nodes_t *reserved_nodes;  // observer
    train_courier_t *train_courier; // observer
    switch_courier_t *switch_courier; // observer

    /**
     * path currently being followed.
     */
    etl::optional<tracks::node_path_reversal_ok_t> path {etl::nullopt};
    /**
     * current node in path. j only tracks sensor node.
     */
    size_t i {}, j {};
    /**
     * my demands to switch after passing this sensor.
     * 
     * steps: after train passed a sensor j, check if it needs to turn any switches ahead.
     * if switching is required, compute them and add to this vector.
     * lock the switches and turn them. after train passed the next sensor, unlock the switches.
     */
    etl::vector<std::tuple<int, switch_dir_t>, 4> switch_demands {};
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
      // interrupted states
      INTERRUPTED_ROUTE_BREAKING,
      INTERRUPTED_ROUTE_WAITING_TO_REVERSE,
    } state {};

    void reset() {
      path = etl::nullopt;
      i = j = 0;
      dest = {};
      braking_dist = 0;
      state = THINKING;
      unlock_my_switches();
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

    void set_switch(int sw, switch_dir_t dir);

    void get_switch_demands();
    void try_switch_on_demand();
    void unlock_my_switches();
    void try_advance_path_sensor();
    void send_switch_locks() const;

    void emergency_stop() {
      reset();
      set_speed(16);
    }

    /**
     * instructs the driver to go to a destination.
     * 
     * does not check for already-existing path.
     */
    void get_path(const track_node *to, int end_offset, tracks::blocked_track_nodes_t additional_blocked = {});

    /**
     * do an action and advances state.
     */
    void perform();

    /**
     * causes train to either slow down or stop to interrupt the traveling path.
     * 
     * target_speed_level needs to be normalized.
     */
    void interrupt_path(int target_speed_level);

    /**
     * instructs driver that road ahead is clear and it can resume its path.
     */
    void interrupt_path_clear();

    /**
     * whether train has a path, but is not moving.
     */
    bool stuck_in_a_path() const {
      return state == INTERRUPTED_ROUTE_BREAKING && train->tick_snap.speed == tracks::fp{};
    }

    /**
     * reverses train and route again.
     */
    void get_reversed_path();
  };
}  // namespace traffic
