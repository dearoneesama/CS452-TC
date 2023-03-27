#pragma once

#include <etl/array.h>
#include <etl/unordered_set.h>
#include <fpm/fixed.hpp>
#include "containers.hpp"
#include "track_new.hpp"

namespace tracks {
  using fp = fpm::fixed_16_16;

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
}
