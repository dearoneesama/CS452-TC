#pragma once

#include <tuple>
#include <etl/optional.h>
#include "tcmd.hpp"
#include "track_consts.hpp"

namespace tracks {
  using switch_dir_t = tcmd::switch_dir_t;
  using switch_status_t = etl::unordered_map<int, switch_dir_t, num_switches>;
  using blocked_track_nodes_t = etl::unordered_set<const track_node *, TRACK_MAX>;

  /**
   * follow track and any turnouts and return the next sensor.
   */
  etl::optional<std::tuple<const track_node *, int>>
  next_sensor(const track_node *curr_sensor, const switch_status_t &switches);

  constexpr size_t max_path_segment_len = TRACK_MAX / 2;
  constexpr size_t max_path_segments = 4;

  using node_path_segment_t = std::tuple<etl::vector<const track_node *, max_path_segment_len>, int>;
  using node_path_reversal_ok_t = etl::vector<node_path_segment_t, max_path_segments>;

  /**
   * computes a shortest path and a distance from source node to destination node.
   * 
   * result is split into segments (acceleration-deceleration regions) with distances required
   * to travel from each starting node.
   * 
   * each segment starts with a sensor node, except the first segment which can start with any node.
   */
  etl::optional<node_path_reversal_ok_t> find_path(
    const track_node *start,
    const track_node *end,
    int start_offset,
    int end_offset,
    const blocked_track_nodes_t &blocked_nodes
  );

  size_t stringify_node_path_segment(char *buf, size_t buflen, const node_path_segment_t &seg);

  template<size_t N>
  etl::string<N> stringify_node_path_segment(const node_path_segment_t &seg) {
    etl::string<N> res;
    auto len = stringify_node_path_segment(res.data(), N, seg);
    res.uninitialized_resize(len);
    return res;
  }

  /**
   * given a switch and a node next to it. which direction of the switch should be taken?
   */
  switch_dir_t get_switch_dir(const track_node *sw, const track_node *next);
}
