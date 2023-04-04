#include <etl/priority_queue.h>
#include "track_graph.hpp"
#include "generic/format.hpp"

namespace tracks {
  etl::optional<std::tuple<const track_node *, int>>
  next_sensor(const track_node *curr_sensor, const switch_status_t &switches) {
    int dist = curr_sensor->edge[DIR_AHEAD].dist;
    curr_sensor = curr_sensor->edge[DIR_AHEAD].dest;
    while (true) {
      if (!curr_sensor) {
        return {};
      }
      switch (curr_sensor->type) {
        case NODE_SENSOR:
          return std::make_tuple(curr_sensor, dist);
        case NODE_MERGE:
          dist += curr_sensor->edge[DIR_AHEAD].dist;
          curr_sensor = curr_sensor->edge[DIR_AHEAD].dest;
          break;
        case NODE_BRANCH: {
          auto dir = switches.at(curr_sensor->num) == traffic::switch_dir_t::S ? DIR_STRAIGHT : DIR_CURVED;
          dist += curr_sensor->edge[dir].dist;
          curr_sensor = curr_sensor->edge[dir].dest;
          break;
        }
        default:
          return {};
      }
    }
  }

  etl::optional<node_path_reversal_ok_t> find_path(
    const track_node *start,
    const track_node *end,
    int start_offset,
    int end_offset,
    const blocked_track_nodes_t &blocked_nodes
  ) {
    struct heap_elem {
      const track_node *node;
      int dist;

      bool operator<(const heap_elem &other) const {
        return dist > other.dist;
      }
    };

    constexpr size_t max_nodes = max_path_segment_len * max_path_segments;
    etl::unordered_map<const track_node *, int, max_nodes> dist;
    etl::unordered_map<const track_node *, const track_node *, max_nodes> prev;
    etl::priority_queue<heap_elem, max_nodes> q;

    dist[start] = 0;
    q.push({start, 0});

    // dijkstra's algorithm
    while (q.size()) {
      auto [n, d] = q.top();
      q.pop();
      // found the end
      if (n == end) {
        break;
      }
      // skip blocked nodes
      if (blocked_nodes.find(n) != blocked_nodes.end()) {
        continue;
      }
      for (size_t i = 0; i < sizeof(n->edge) / sizeof(n->edge[0]); ++i) {
        auto &edge = n->edge[i];
        // NODE_EXIT, ...
        if (!edge.dest) {
          continue;
        }
        auto alt = d + edge.dist;
        if (dist.find(edge.dest) == dist.end() || alt < dist[edge.dest]) {
          dist[edge.dest] = alt;
          prev[edge.dest] = n;
          q.push({edge.dest, alt});
        }
      }
      // can reverse only on sensor nodes
      if (n->type == NODE_SENSOR) {
        auto *rev = n->reverse;
        if (dist.find(rev) == dist.end() || d < dist[rev]) {
          dist[rev] = d;
          prev[rev] = n;
          q.push({rev, d});
        }
      }
    }

    if (dist.find(end) == dist.end()) {
      return {};
    }

    // reconstruct path
    node_path_reversal_ok_t result {};
    result.emplace_back();
    size_t idx = 0;
    auto *segment = &std::get<0>(result.at(idx));
    const track_node *curr = end;
    while (curr != start) {
      segment->push_back(curr);
      auto *last = prev.at(curr);
      if (curr->reverse == last) {
        std::reverse(segment->begin(), segment->end());
        ++idx;
        result.emplace_back();
        segment = &std::get<0>(result.at(idx));
      } else if (last->type == NODE_BRANCH) {
        auto dir = get_switch_dir(last, curr) == traffic::switch_dir_t::S ? DIR_STRAIGHT : DIR_CURVED;
        std::get<1>(result.at(idx)) += last->edge[dir].dist;
      } else {
        std::get<1>(result.at(idx)) += last->edge[DIR_AHEAD].dist;
      }
      curr = last;
    }
    segment->push_back(curr);
    std::reverse(segment->begin(), segment->end());
    std::reverse(result.begin(), result.end());

    std::get<1>(result.at(0)) -= start_offset;
    std::get<1>(result.back()) += end_offset;

    return result;
  }

  size_t stringify_node_path_segment(char *buf, size_t buflen, const node_path_segment_t &seg) {
    auto &vec = std::get<0>(seg);
    auto len = troll::snformat(buf, buflen, "(");
    size_t i;
    for (i = 0; i < vec.size() - 1; ++i) {
      len += troll::snformat(buf + len, buflen - len, "{},", vec.at(i)->name);
    }
    len += troll::snformat(buf + len, buflen - len, "{};{})", vec.at(i)->name, std::get<1>(seg));
    return len;
  }

  traffic::switch_dir_t get_switch_dir(const track_node *sw, const track_node *next) {
    return sw->edge[DIR_STRAIGHT].dest == next ? traffic::switch_dir_t::S : traffic::switch_dir_t::C;
  }
}
