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

  void mini_driver::set_switch(int sw, bool is_straight) {
    utils::enumed_class msg {
      sw,
      is_straight ? tcmd::switch_dir_t::S : tcmd::switch_dir_t::C,
    };
    switch_courier->push(msg);
  }

  void mini_driver::switch_ahead_on_demand() {
    auto &segment = this->segment();
    // assume j is at a sensor
    for (size_t x = this->j + 1; x < segment.size() - 1; ++x) {
      if (segment[x]->type == NODE_BRANCH) {
        ui::out().send_notice(troll::sformat<30>("Need to switch BR{}.", segment[x]->num));
        set_switch(segment[x]->num, segment[x]->edge[DIR_STRAIGHT].dest == segment[x + 1]);
      } else if (segment[x]->type == NODE_MERGE) {
        continue;
      } else {
        break;
      }
    }
  }

  void mini_driver::perform() {
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
      switch_ahead_on_demand();
      // reserve ALL nodes
      for (auto const *node : segment()) {
        reserved_nodes->insert(node);
      }
      state = ENROUTE_RUNNING_SEGMENT;
      set_speed(speed + 16);
      break;
    }

    case ENROUTE_RUNNING_SEGMENT: {
      if (segment_dist_left() <= braking_dist) {
        state = ENROUTE_BREAKING;
        set_speed(16);
      }
      // update index into the path segment
      auto &segment = this->segment();
      if (static_cast<etl::string_view>(segment.at(j)->name) != train->tick_snap.pos.name) {
        auto nextit = std::find_if(segment.begin() + j + 1, segment.end(), [](auto const *n) {
          return n->type == NODE_SENSOR;
        });
        if (nextit != segment.end()) {
          j = nextit - segment.begin();
          switch_ahead_on_demand();
        }
      }
      break;
    }

    case ENROUTE_BREAKING: {
      if (train->tick_snap.speed != fp{}) {
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
      // TODO: this is not right as nodes beneath the train are also freed
      for (auto const *node : segment()) {
        reserved_nodes->erase(node);
      }
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

    default:
      break;
    }
  }
}  // namespace traffic
