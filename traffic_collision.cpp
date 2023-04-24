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

  bool collision_avoider::handle_train(mini_driver &driver) {
    bool changed = false;
    auto &tr = *driver.train;
    auto &my_locks = train_node_locks.at(tr.num);

    internal_train_state *tr2_rearended = nullptr, *tr2_opposite = nullptr;

    // find conflicting train
    for (auto &[num, their_locks] : train_node_locks) {
      if (num == tr.num || !their_locks.size()) {
        continue;
      }
      for (auto *node : my_locks) {
        // front is behind node, back is in front of node
        if (node == their_locks.front()) {
          //ui::out().send_notice(troll::sformat<40>("Train {} might rear-end {}.", tr.num, num));
          Yield();
          tr2_rearended = drivers->at(num).train;
        } else if (node == their_locks.back()->reverse) {
          //ui::out().send_notice(troll::sformat<40>("Train {} might head on with {}.", tr.num, num));
          Yield();
          tr2_opposite = drivers->at(num).train;
        }
        if (tr2_rearended && tr2_opposite) {
          break;
        }
      }
    }

    bool was_clear = cleared_trains.count(tr.num);
    if (!tr2_rearended && !tr2_opposite && !was_clear) {
      // no collision possible, try clear the path if driver is interrupted
      changed |= driver.interrupt_path_clear();
      cleared_trains.insert(tr.num);
      return changed;
    } else if (!tr2_rearended && !tr2_opposite) {
      return changed;
    }
    cleared_trains.erase(tr.num);

    // rear end / common switch
    if (tr2_rearended) {
      // slow down using a decrement
      auto curr_speed = tr.cmd >= 16 ? tr.cmd - 16 : tr.cmd;
      changed |= driver.interrupt_path(etl::max(0, curr_speed - 2));
    }
    // head on
    if (tr2_opposite) {
      // stop!
      // the other guy will stop too
      changed |= driver.interrupt_path(0);
    }
    // both stopped
    auto *tr2 = tr2_rearended ? tr2_rearended : tr2_opposite;
    if (tr2->tick_snap.speed == fp{} && driver.stuck_in_a_path()) {
      // reverse me to clear the other
      changed |= driver.get_reversed_path();
    }
    return changed;
  }

  void collision_avoider::perform() {
    get_new_sensor_locks();
    send_sensor_locks();
    for (auto *tr : *initialized_trains) {
      auto &dr = drivers->at(tr->num);
      if (handle_train(dr)) {
        get_new_sensor_locks();
        send_sensor_locks();
      }
    }
  }
}  // namespace traffic
