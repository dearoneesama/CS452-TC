#pragma once

#include <etl/array.h>
#include <etl/unordered_set.h>
#include <fpm/fixed.hpp>
#include "generic/containers.hpp"
#include "track_new.hpp"

namespace tracks {
  using fp = fpm::fixed_24_8;

  static constexpr size_t num_switches = 22;
  static constexpr size_t num_trains = 6;

  /**
   * a static array of switch names.
   */
  etl::array<int, num_switches> const &valid_switches();

  /**
   * a static set of broken, but valid switches.
   */
  etl::unordered_set<int, num_switches> const &broken_switches();

  /**
   * a static array of train numbers.
   */
  etl::array<int, num_trains> const &valid_trains();

  /**
   * a static adjacency list of track nodes.
   */
  troll::string_map<const track_node *, 4, TRACK_MAX> const &valid_nodes();

  /**
   * seeds of constant train speeds at different speed levels.
   */
  fp &train_speed(int train, int target_speed_level, int old_speed_level);

  /**
   * seeds of constant train accelerations at different speed levels.
   * 
   * RESULT is absolute value of acceleration.
   */
  fp &train_acceleration(int train, int target_speed_level, int old_speed_level);

  /**
   * acceleration distance: the distance a train needs to travel to reach constant
   * speed from 0 given speed level.
   * 
   * braking distance: the distance a train needs to travel to stop from constant
   * speed level.
   */
  std::tuple<int, int> accel_deaccel_distance(int train, int steady_speed_level);

  /**
   * time required for acceleration and braking.
   */
  std::tuple<fp, fp> accel_deaccel_time(int train, int steady_speed_level);

  /**
   * find a suitable speed level such that train can accelerate and brake, while the total
   * distance for them is less than or equal to `distance`.
   * 
   * this speed is normalized to 1-14.
   */
  int find_max_speed_level_for_dist(int train, int distance);
}
