#include "track_graph.hpp"

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
          auto dir = switches.at(curr_sensor->num) == switch_dir_t::S ? DIR_STRAIGHT : DIR_CURVED;
          dist += curr_sensor->edge[dir].dist;
          curr_sensor = curr_sensor->edge[dir].dest;
          break;
        }
        default:
          return {};
      }
    }
  }
}
