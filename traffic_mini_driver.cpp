#include "traffic_mini_driver.hpp"
#include "ui.hpp"

using namespace tracks;
using namespace traffic;

namespace {
  bool asked_to_stop_at_sensor(const mini_driver &driver) {
    // either preset offset is 0, or we're not at the last node in the path
    // (we always split paths at sensors)
    return (driver.dest.offset == 0 || driver.i < driver.path->size() - 1)
    // do not want to "fix" if we are not displacing..
      && driver.segment().size() != 1;
  }

  int find_speed_for_inst_accel(int train) {
    return find_max_speed_level_for_dist(train, 100);
  }
}  // namespace

namespace traffic {
  void mini_driver::set_speed(int speed) {
    utils::enumed_class msg {
      tcmd::tc_msg_header::SPEED,
      tcmd::speed_cmd { train->num, speed },
    };
    train_courier->push(msg);
  }

  void mini_driver::set_switch(int sw, switch_dir_t dir) {
    switch_cmd msg { sw, dir };
    switch_courier->push(msg);
  }

  void mini_driver::get_switch_demands() {
    auto &segment = this->segment();
    // demand switches up to next next sensor if too fast
    auto first = train->tick_snap.speed > fp{50};
    for (size_t x = this->j + 1; x < segment.size() - 1; ++x) {
      if (segment[x]->type == NODE_BRANCH) {
        auto dir = get_switch_dir(segment[x], segment[x + 1]);
        if (switches->status[segment[x]->num] != dir) {
          switch_demands.push_back({segment[x]->num, dir});
        }
      } else if (segment[x]->type == NODE_MERGE) {
        continue;
      } else if (!first) {
        first = true;
      } else {
        break;
      }
    }
  }

  void mini_driver::try_switch_on_demand() {
    for (auto &[num, dir] : switch_demands) {
      // switched already || locked by other train
      if (dir == switch_dir_t::NONE || switches->locks.at(num)) {
        continue;
      }
      switches->locks.at(num) = train->num;
      set_switch(num, dir);
      dir = switch_dir_t::NONE;
    }
  }

  void mini_driver::unlock_my_switches() {
    for (auto &[num, dir] : switch_demands) {
      if (switches->locks.at(num) == train->num) {
        switches->locks.at(num) = 0;
      }
    }
    switch_demands.clear();
  }

  void mini_driver::try_advance_path_sensor() {
    // update index into the path segment
    auto &segment = this->segment();
    // if train's current sensor changed
    if (etl::string_view{segment.at(j)->name} != train->tick_snap.pos.name) {
      auto wanted_it = std::find_if(segment.begin(), segment.end(), [&](auto const *n) {
        return etl::string_view{n->name} == train->tick_snap.pos.name;
      });
      if (wanted_it == segment.end()) {
        ui::out().send_notice(troll::sformat<30>("Train {} lost path. Stop.", train->num));
        emergency_stop();
        return;
      }
      auto nextit = std::find_if(segment.begin() + j + 1, segment.end(), [](auto const *n) {
        return n->type == NODE_SENSOR;
      });
      if (nextit != segment.end()) {
        j = nextit - segment.begin();
        unlock_my_switches();
        get_switch_demands();
        try_switch_on_demand();
      }
    }
  }

  void mini_driver::send_switch_locks() const {
    etl::string<20> str;
    for (auto &[num, dir] : switch_demands) {
      if (switches->locks.at(num) == train->num) {
        str.append(troll::sformat<10>("BR{}-", num));
      }
    }
    if (str.back() == '-') {
      str.uninitialized_resize(str.size() - 1);
    }
    ui::out().send_value(utils::enumed_class {
      ui::display_msg_header::SWITCH_LOCK,
      ui::switch_lock { train->num, str },
    });
  }

  void mini_driver::get_path(const track_node *to, int end_offset, blocked_track_nodes_t additional_blocked) {
    auto *from = valid_nodes().at(train->tick_snap.pos.name);
    auto start_offset = train->tick_snap.pos.offset;
    additional_blocked.assign(reserved_nodes->begin(), reserved_nodes->end());
    path = find_path(from, to, start_offset, end_offset, additional_blocked);
    if (!path) {
      ui::out().send_notice(troll::sformat<128>(
        "Free path finding for train {} was not possible: {} to {}. Routing failed.",
        train->num,
        ui::stringify_pos(position_t { from->name, start_offset }),
        ui::stringify_pos(position_t { to->name, end_offset })
      ));
      return;
    }

    ui::out().send_notice(troll::sformat<128>(
      "Path found for train {}: {} to {}. Length is {}.",
      train->num,
      ui::stringify_pos(position_t { from->name, start_offset }),
      ui::stringify_pos(position_t { to->name, end_offset }),
      std::accumulate(path->begin(), path->end(), 0, [](int acc, auto &&seg) {
        return acc + std::get<1>(seg);
      })
    ));

    i = j = 0;
    state = ENROUTE_ACCEL_TO_START_SEGMENT;
    dest = { to->name, end_offset };
  }

  void mini_driver::perform() {
    send_switch_locks();
    switch (state) {
    case ENROUTE_ACCEL_TO_START_SEGMENT: {
      auto speed = find_max_speed_level_for_dist(train->num, segment_dist_left());
      braking_dist = std::get<1>(accel_deaccel_distance(train->num, speed));
      ui::out().send_notice(troll::sformat<147>(
        "Train {} will try to take path {} at speed {} (+16). BD {}.",
        train->num,
        stringify_node_path_segment<100>((*path)[i]),
        speed,
        braking_dist
      ));
      get_switch_demands();
      try_switch_on_demand();
      state = ENROUTE_RUNNING_SEGMENT;
      set_speed(speed + 16);
      break;
    }

    case ENROUTE_RUNNING_SEGMENT: {
      try_switch_on_demand();
      if (segment_dist_left() <= braking_dist) {
        state = ENROUTE_BREAKING;
        set_speed(16);
      }
      // update index into the path segment
      try_advance_path_sensor();
      break;
    }

    case ENROUTE_BREAKING: {
      if (train->tick_snap.speed != fp{}) {
        try_advance_path_sensor();
        break;
      }
      // train has stopped, but might not at correct location..
      if (asked_to_stop_at_sensor(*this)) {
        auto *target_sensor = segment().back();
        if (target_sensor->type != NODE_SENSOR) {
          goto dont_need_to_fix;
        }
        if (static_cast<etl::string_view>(train->tick_snap.pos.name) == target_sensor->name) {
          // passed the sensor
          if (train->sitting_on_sensor) {
            goto dont_need_to_fix;
          }
          // fix by backing up
          reverse_timer = 0;
          state = FIX_WAITING_TO_REVERSE_1;
          ui::out().send_notice(troll::sformat<40>("Train {} needs to reverse to sensor.", train->num));
          set_speed(15);
        } else {
          // have not reached sensor
          state = FIX_RUNNING_UP;
          ui::out().send_notice(troll::sformat<43>("Train {} needs to go farther to sensor.", train->num));
          set_speed(find_speed_for_inst_accel(train->num) + 16);
        }
      } else {
      dont_need_to_fix:
        state = ENROUTE_END_SEGMENT;
      }
      break;
    }

    case ENROUTE_END_SEGMENT: {
      unlock_my_switches();
      if (i == path->size() - 1) {
        ui::out().send_notice(troll::sformat<128>(
          "Train {} has reached destination {}.",
          train->num,
          segment().back()->name
        ));
        path = etl::nullopt;
        dest = {};
        state = THINKING;
      } else {
        // need to reverse and then begin next segment
        state = ENROUTE_WAITING_TO_REVERSE;
        ++i;
        j = 0;
        reverse_timer = 0;
        set_speed(15);
      }
      break;
    }

    case ENROUTE_WAITING_TO_REVERSE: {
      ++reverse_timer;
      if (reverse_timer >= 100 / predict_react_interval) {
        state = ENROUTE_ACCEL_TO_START_SEGMENT;
      }
      break;
    }

    // fix-sensor procedures
    case FIX_RUNNING_UP:
      if (train->sitting_on_sensor) {
        state = ENROUTE_END_SEGMENT;
        set_speed(16);
      }
      break;

    case FIX_WAITING_TO_REVERSE_1:
      ++reverse_timer;
      if (reverse_timer >= 100 / predict_react_interval) {
        state = FIX_RUNNING_BACK;
        set_speed(find_speed_for_inst_accel(train->num) + 16);
      }
      break;

    case FIX_RUNNING_BACK:
      if (train->sitting_on_sensor) {
        reverse_timer = 0;
        state = FIX_WAITING_TO_REVERSE_2;
        set_speed(15);
      }
      break;

    case FIX_WAITING_TO_REVERSE_2:
      ++reverse_timer;
      if (reverse_timer >= 100 / predict_react_interval) {
        state = ENROUTE_END_SEGMENT;
      }
      break;

    // interrupt procedures
    case INTERRUPTED_ROUTE_BREAKING:
      break;

    case INTERRUPTED_ROUTE_WAITING_TO_REVERSE:
      ++reverse_timer;
      if (reverse_timer >= 100 / predict_react_interval) {
        // do not reverse back
        get_path(valid_nodes().at(dest.name), dest.offset, {valid_nodes().at(train->tick_snap.pos.name)->reverse});
      }
      break;

    default:
      break;
    }
  }

  bool mini_driver::interrupt_path(int target_speed_level) {
    if (train->tick_snap.speed == fp{}) {
      return false;
    }
    if (target_speed_level == 0) {
      ui::out().send_notice(troll::sformat<60>(
        "Train {} needs to stop as interrupted.",
        train->num
      ));
      set_speed(16);
      if (state == ENROUTE_RUNNING_SEGMENT) {
        state = INTERRUPTED_ROUTE_BREAKING;
      }
    } else {
      ui::out().send_notice(troll::sformat<70>(
        "Train {} needs to change speed to {} (+16) as interrupted.",
        train->num,
        target_speed_level
      ));
      set_speed(target_speed_level + 16);
      if (state == ENROUTE_RUNNING_SEGMENT) {
        braking_dist = std::get<1>(accel_deaccel_distance(train->num, target_speed_level));
      }
    }
    return true;
  }

  bool mini_driver::interrupt_path_clear() {
    if (!path || (state != ENROUTE_RUNNING_SEGMENT && state != INTERRUPTED_ROUTE_BREAKING)) {
      return false;
    }
    auto speed = find_max_speed_level_for_dist(train->num, segment_dist_left());
    auto new_braking_dist = std::get<1>(accel_deaccel_distance(train->num, speed));
    if (new_braking_dist != braking_dist) {
      braking_dist = new_braking_dist;
      ui::out().send_notice(troll::sformat<70>(
        "Train {} needs to change speed to {} (+16) as road is clear.",
        train->num,
        speed
      ));
      set_speed(speed + 16);
    }
    state = ENROUTE_RUNNING_SEGMENT;
    return true;
  }

  bool mini_driver::get_reversed_path() {
    if (!stuck_in_a_path() || state == INTERRUPTED_ROUTE_WAITING_TO_REVERSE) {
      return false;
    }
    unlock_my_switches();
    reverse_timer = 0;
    set_speed(15);
    state = INTERRUPTED_ROUTE_WAITING_TO_REVERSE;
    return true;
  }
}  // namespace traffic
