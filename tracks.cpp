#include "tracks.hpp"
#include "user_syscall_typed.hpp"
#include "ui.hpp"
#include "track_consts.hpp"
#include "track_graph.hpp"

namespace tracks {

  /**
   * information about a train.
   * 
   * units for distances are mm.
   * units for times are second.
   */
  struct internal_train_state {
    // train number
    int num {};
    // last speed command given to it
    int cmd {};
    // low speed to high speed?
    bool lo_to_hi {};

    struct snapshot_t {
      position_t pos {};
      // positive
      fp speed {};
      // positive for acceleration, negative for deceleration
      fp accel {};
      unsigned tick {};
    };

    // information based on last sensor reading
    snapshot_t sensor_snap {};
    // sensor snap error terms
    fp sn_delta_t {}, sn_delta_d {};
    // train ever received speed change command since last sensor?
    bool sn_accelerated {};
    // average speed across sensor
    troll::pseudo_moving_average<fp> sn_avg_speed {};

    // information based on last tick
    // the two fields will be in sync once train hits a sensor
    snapshot_t tick_snap {};

    // train is approaching source sensor instead of leaving?
    bool approaching_sensor {};
  };

  /**
   * collection of information used to drive the track server.
   */
  struct track_state {
    track_state() {
      for (auto i : valid_trains()) {
        trains[i].num = {i};
      }
      for (auto i : valid_switches()) {
        switches[i] = switch_dir_t::NONE;
      }
    }

    static ui::train_read make_train_ui_msg(const internal_train_state &train) {
      return {
        train.num,
        train.cmd,
        {},  // dest
        train.tick_snap.pos,  // curr pos
        train.tick_snap.speed,  // curr speed
        train.sn_delta_t,
        train.sn_delta_d,
      };
    }

    static void send_train_ui_msg(const internal_train_state &train, tid_t display_controller) {
      utils::enumed_class msg {
        ui::display_msg_header::TRAIN_READ,
        make_train_ui_msg(train),
      };
      SendValue(display_controller, msg, null_reply);
    }

    void handle_speed_cmd(speed_cmd &cmd, tid_t display_controller) {
      auto &train = trains.at(cmd.train);

      if (cmd.speed == train.cmd) {
        return;
      }

      train.sn_accelerated = true;
      if (cmd.speed == 15) {
        train.lo_to_hi = false;
        train.approaching_sensor = !train.approaching_sensor;
      } else if (train.cmd == 15) {
        train.lo_to_hi = true;
      } else {
        auto new_speed = train_speed(train.num, cmd.speed, train.cmd);
        auto old_speed = train.sensor_snap.speed;
        train.lo_to_hi = new_speed >= old_speed;
      }

      if (train.lo_to_hi) {
        train.tick_snap.accel = train_acceleration(cmd.train, cmd.speed, train.cmd);
      } else {
        train.tick_snap.accel = -train_acceleration(cmd.train, cmd.speed, train.cmd);
      }
      train.cmd = cmd.speed;
      // package information and send to display controller
      send_train_ui_msg(train, display_controller);
    }

    void handle_switch_cmd(switch_cmd &cmd, tid_t display_controller) {
      switches.at(cmd.switch_num) = cmd.switch_dir;
      // forward to display controller
      utils::enumed_class msg {
        ui::display_msg_header::SWITCHES,
        ui::switch_read { cmd.switch_num, cmd.switch_dir },
      };
      SendValue(display_controller, msg, null_reply);
    }

    void handle_sensor_read(sensor_read &read, tid_t display_controller) {
      for (auto &pair : trains) {
        if (adjust_train_location_from_sensor(pair.second, read)) {
          send_train_ui_msg(pair.second, display_controller);
        }
      }
      // forward to display controller
      utils::enumed_class msg {
        ui::display_msg_header::SENSOR_MSG,
        ui::sensor_read { read.sensor, read.tick },
      };
      SendValue(display_controller, msg, null_reply);
    }

    bool adjust_train_location_from_sensor(internal_train_state &train, sensor_read &read) {
      // assume every train must have a preset location
      if (!train.sensor_snap.pos.name.size()) {
        return false;
      }
      auto *train_node = valid_nodes().at(train.sensor_snap.pos.name);
      auto *sensor_node = valid_nodes().at(read.sensor);

      auto reset_snaps = [&train, &read, &sensor_node] {
        // sync sensor snap with tick snap
        train.sensor_snap.pos.name = sensor_node->name;
        train.sensor_snap.pos.offset = 0;
        train.sensor_snap.accel = train.tick_snap.accel;
        train.sensor_snap.speed = train.tick_snap.speed;
        train.sensor_snap.tick = read.tick;
        train.tick_snap = train.sensor_snap;
        // reset some states
        train.sn_accelerated = false;
        train.approaching_sensor = false;
        train.sn_avg_speed = {};
      };

      // doing this to ensure: if train is stopped at sensor, then when it restarts, it will
      // have correct time tick. but if train is just passing by the sensor very slowly, just
      // ignore it.
      if (train.sensor_snap.pos.name == read.sensor) {
        if (train.sensor_snap.accel == fp{} && train.tick_snap.speed == fp{}) {
          // train is stopped at sensor
          train.sensor_snap.tick = train.tick_snap.tick = read.tick;
          return true;
        } else {
          return false;
        }
      }

      // handle a special case first: train leaves sensor, then reverses and so it comes back!
      if (train_node->reverse == sensor_node) {
        reset_snaps();
        // dd and dt are not reportable in this case
        train.sn_delta_d = train.sn_delta_t = fp{999999} / 100;
        return true;
      }

      // error tolerance is the distance from one sensor to next, so if two sensors are not
      // adjacent, then return.
      auto train_next = next_sensor(train_node, switches);
      // end of track or not expected sensor
      if (!train_next || sensor_node != std::get<0>(*train_next)) {
        return false;
      }

      // need to report dt and dd
      auto expected_d = fp(std::get<1>(*train_next));
      auto actual_d = fp(train.tick_snap.pos.offset);
      auto actual_t = fp(train.tick_snap.tick - train.sensor_snap.tick) / 100;
      auto expected_t = expected_d / train.sn_avg_speed.value();
      train.sn_delta_d = actual_d - expected_d;
      train.sn_delta_t = actual_t - expected_t;

      // make correction to train's speed, if there is no acceleration occurring
      if (!train.sn_accelerated && train.cmd) {
        const auto alpha = fp{0.3};
        auto &standard_v = train.lo_to_hi ? train_speed(train.num, train.cmd, 0) : train_speed(train.num, train.cmd, 14);
        auto new_v = alpha * train.tick_snap.speed + (fp{1} - alpha) * actual_d / actual_t;
        train.tick_snap.speed = standard_v = new_v;
      }

      reset_snaps();
      return true;
    }

    /**
     * updates trains' locations and speeds based on estimations.
     */
    void handle_train_predict(int current_tick, tid_t display_controller) {
      for (auto &[num, train] : trains) {
        // short path
        if ((train.tick_snap.accel == fp{} && train.tick_snap.speed == fp{}) || !train.sensor_snap.pos.name.size()) {
          train.sn_avg_speed.add({});
          continue;
        }
        auto dt = fp(current_tick - train.tick_snap.tick) / 100;
        auto new_v = train.tick_snap.speed + train.tick_snap.accel * dt;

        // clamp speed to max values if acceleration is complete
        if (train.tick_snap.accel != fp{}) {
          if (train.lo_to_hi) {
            auto target_v = train_speed(train.num, train.cmd, 0);
            if (new_v >= target_v) {
              new_v = target_v;
              train.tick_snap.accel = {};
            }
          } else {
            auto target_v = train_speed(train.num, train.cmd, 14);
            if (new_v <= target_v) {
              new_v = target_v;
              train.tick_snap.accel = {};
            }
          }
        }
        // sloppy estimation: s = v0t + 1/2at^2 because our t is small
        auto ddist = static_cast<int>(train.tick_snap.speed * dt + train.tick_snap.accel * dt * dt / 2);
        if (train.approaching_sensor) {
          train.tick_snap.pos.offset -= ddist;
        } else {
          train.tick_snap.pos.offset += ddist;
        }
        train.tick_snap.speed = new_v;
        train.tick_snap.tick = current_tick;
        train.sn_avg_speed.add(new_v);
        send_train_ui_msg(train, display_controller);
      }
    }

    void handle_train_pos_init(const train_pos_init_msg &msg, tid_t display_controller) {
      auto &train = trains.at(msg.train);
      auto old_cmd = train.cmd;
      train = {};
      train.num = msg.train;
      train.cmd = old_cmd;
      train.sensor_snap.pos.name = train.tick_snap.pos.name = msg.name;
      send_train_ui_msg(train, display_controller);
    }

    // information that is probably not good for credit if static
    etl::unordered_map<int, internal_train_state, num_trains> trains {};
    etl::unordered_map<int, switch_dir_t, num_switches> switches {};
  };

  void track_server() {
    track_state state;
    RegisterAs(TRACK_SERVER_TASK_NAME);
    auto display_controller = TaskFinder(ui::DISPLAY_CONTROLLER_NAME);
    auto clock_server = TaskFinder("clock_server");

    utils::enumed_class<track_msg_header, char[128]> msg;
    tid_t request_tid;

    while (true) {
      if (ReceiveValue(request_tid, msg) < 1) {
        continue;
      }
      switch (msg.header) {
      case track_msg_header::TRAIN_SPEED_CMD:
        ReplyValue(request_tid, null_reply);
        state.handle_speed_cmd(msg.data_as<speed_cmd>(), display_controller());
        break;
      case track_msg_header::SWITCH_CMD:
        ReplyValue(request_tid, null_reply);
        state.handle_switch_cmd(msg.data_as<switch_cmd>(), display_controller());
        break;
      case track_msg_header::SENSOR_READ:
        ReplyValue(request_tid, null_reply);
        state.handle_sensor_read(msg.data_as<sensor_read>(), display_controller());
        break;
      case track_msg_header::TRAIN_PREDICT:
        state.handle_train_predict(Time(clock_server()), display_controller());
        ReplyValue(request_tid, null_reply);
        break;
      case track_msg_header::TRAIN_POS_INIT:
        ReplyValue(request_tid, track_reply_msg::OK);
        state.handle_train_pos_init(msg.data_as<train_pos_init_msg>(), display_controller());
        break;
      default:
        break;
      }
    }
  }

  void predict_timer() {
    auto track_server = TaskFinder(TRACK_SERVER_TASK_NAME);
    auto clock_server = TaskFinder("clock_server");
    while (true) {
      Delay(clock_server(), 7);  // update every 70ms
      SendValue(track_server(), track_msg_header::TRAIN_PREDICT, null_reply);
    }
  }

  void init_tasks() {
    Create(priority_t::PRIORITY_L1, predict_timer);
    Create(priority_t::PRIORITY_L1, track_server);
  }

} // namespace tracks
