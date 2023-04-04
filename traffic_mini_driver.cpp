#include "traffic_mini_driver.hpp"
#include "ui.hpp"

using namespace tracks;

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

  void mini_driver::perform() {
    switch (state) {
    case ACCEL_TO_START_SEGMENT: {
      auto speed = find_max_speed_level_for_dist(train->num, segment_dist_left());
      braking_dist = std::get<1>(accel_deaccel_distance(train->num, speed));
      ui::out().send_notice(troll::sformat<147>(
        "Train {} will try to take path {} at speed {} (+16). BD {}.",
        train->num,
        stringify_node_path_segment<100>((*path)[i]),
        speed,
        braking_dist
      ));
      // turn ALL switches
      auto &segment = this->segment();
      for (size_t i = 0; i < segment.size() - 1; ++i) {
        if (segment[i]->type == NODE_BRANCH) {
          set_switch(segment[i]->num, segment[i]->edge[DIR_STRAIGHT].dest == segment[i + 1]);
        }
      }
      // reserve ALL nodes
      for (auto const *node : segment) {
        reserved_nodes->insert(node);
      }
      state = RUNNING_SEGMENT;
      set_speed(speed + 16);
      break;
    }
    case RUNNING_SEGMENT: {
      if (segment_dist_left() <= braking_dist) {
        state = BREAKING;
        set_speed(16);
      }
      break;
    }
    case BREAKING: {
      if (train->tick_snap.speed == fp{}) {
        state = END_SEGMENT;
      }
      break;
    }
    case END_SEGMENT: {
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
        state = WAITING_TO_REVERSE;
        ++i;
        reverse_timer = 0;
        set_speed(15);
      }
      break;
    }
    case WAITING_TO_REVERSE: {
      ++reverse_timer;
      if (reverse_timer >= 100 / predict_react_interval) {
        state = ACCEL_TO_START_SEGMENT;
      }
      break;
    }
    case ACCEL_TO_FIX_SENSOR:
      break;
    case FIXING_SENSOR:
      break;
    default:
      break;
    }
  }
}  // namespace traffic
