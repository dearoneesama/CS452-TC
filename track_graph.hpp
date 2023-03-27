#pragma once

#include <tuple>
#include <etl/optional.h>
#include "track_consts.hpp"
#include "tracks.hpp"

namespace tracks {
  using switch_status_t = etl::unordered_map<int, switch_dir_t, num_switches>;
  /**
   * follow track and any turnouts and return the next sensor.
   */
  etl::optional<std::tuple<const track_node *, int>>
  next_sensor(const track_node *curr_sensor, const switch_status_t &switches);
}
