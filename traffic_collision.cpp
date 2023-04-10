#include "traffic_collision.hpp"
#include "ui.hpp"

using namespace tracks;
using namespace traffic;

namespace traffic {
  void collision_avoider::get_new_sensor_locks() {
    for (auto *tr : *initialized_trains) {
      auto &dr = drivers->at(tr->num);
      auto braking_dist = std::get<1>(accel_deaccel_distance(tr->num, tr->cmd));
      if (dr.path) {
        auto &segment = dr.segment();
        // dr.j == tick_snap.pos.name here
        train_node_locks[tr->num] = walk_sensor(dr.j, tr->tick_snap.pos.offset, braking_dist, segment, *switches);
      } else {
        // may be stationary, or cruising randomly, etc.
        auto *start = valid_nodes().at(tr->tick_snap.pos.name);
        train_node_locks[tr->num] = walk_sensor(start, tr->tick_snap.pos.offset, braking_dist, *switches);
      }
      //auto &range = train_node_locks[tr->num];
      //ui::out().send_notice(troll::sformat<50>("Train {} locks {} -> {}", tr->num, range.front()->name, range.back()->name));
    }
  }

  void collision_avoider::send_sensor_locks() const {
    for (auto *tr : *initialized_trains) {
      etl::string<20> str;
      auto &range = train_node_locks.at(tr->num);
      for (size_t i = 0; i < range.size(); ++i) {
        str.append(range[i]->name);
        if (i != range.size() - 1) {
          str.append("-");
        }
      }
      ui::out().send_value(utils::enumed_class {
        ui::display_msg_header::SENSOR_LOCK,
        ui::sensor_lock { tr->num, str }
      });
    }
  }

  void collision_avoider::handle_train(mini_driver &driver) {
    auto &tr = *driver.train;
    auto &my_locks = train_node_locks.at(tr.num);

    internal_train_state *tr2_rearended = nullptr, *tr2_opposite = nullptr;

    // find conflicting train
    for (auto &[num, their_locks] : train_node_locks) {
      if (num == tr.num) {
        continue;
      }
      for (auto *their_node : their_locks) {
        for (auto *node : my_locks) {
          if (node == their_node) {
            ui::out().send_notice(troll::sformat<40>("Train {} might rear-end {}.", tr.num, num));
            tr2_rearended = drivers->at(num).train;
          } else if (node == their_node->reverse) {
            ui::out().send_notice(troll::sformat<40>("Train {} might head on with {}.", tr.num, num));
            tr2_opposite = drivers->at(num).train;
          }
          if (tr2_rearended && tr2_opposite) {
            break;
          }
        }
      }
    }

    bool was_clear = cleared_trains.count(tr.num);
    if (!tr2_rearended && !tr2_opposite && !was_clear) {
      // no collision possible, try clear the path if driver is interrupted
      driver.interrupt_path_clear();
      cleared_trains.insert(tr.num);
      return;
    } else if (!tr2_rearended && !tr2_opposite) {
      return;
    }
    cleared_trains.erase(tr.num);

    // rear end / common switch
    if (tr2_rearended) {
      // slow down using a decrement
      auto curr_speed = tr.cmd >= 16 ? tr.cmd - 16 : tr.cmd;
      driver.interrupt_path(etl::max(0, curr_speed - 2));
    }
    // head on
    if (tr2_opposite) {
      // stop!
      // the other guy will stop too
      driver.interrupt_path(0);
    }
    // both stopped
    auto *tr2 = tr2_rearended ? tr2_rearended : tr2_opposite;
    if (tr2->tick_snap.speed == fp{} && driver.stuck_in_a_path()) {
      // reverse me to clear the other
      driver.get_reversed_path();
    }
  }

  void collision_avoider::perform() {
    get_new_sensor_locks();
    send_sensor_locks();
    for (auto *tr : *initialized_trains) {
      auto &dr = drivers->at(tr->num);
      handle_train(dr);
    }
  }
}  // namespace traffic
