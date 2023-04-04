#include "traffic.hpp"
#include "kern/user_syscall_typed.hpp"
#include "ui.hpp"
#include "track_consts.hpp"
#include "track_graph.hpp"

using namespace tracks;

namespace traffic {

  /**
   * how many ticks to wait before assuming a train has left a sensor.
   */
  static constexpr auto train_sensor_expire_timeout = 50;

  static constexpr auto predict_react_interval = 15;  // 150 ms

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
    // train is on the sensor thus spamming lots of sensor reads?
    bool sitting_on_sensor {};
  };

  using train_courier_t = utils::courier_runner<
    traffic_msg_header::TO_TC_COURIER,
    sizeof(utils::enumed_class<tcmd::tc_msg_header, tcmd::speed_cmd>)
  >;
  using switch_courier_t = utils::courier_runner<traffic_msg_header::TO_SWITCH_COURIER, sizeof(switch_cmd)>;

  /**
   * simulates the driver of one train.
   */
  struct mini_driver {
    /**
     * dependencies
     */
    internal_train_state *train;  // observer
    blocked_track_nodes_t *reserved_nodes;  // observer
    train_courier_t *train_courier; // observer
    switch_courier_t *switch_courier; // observer

    /**
     * path currently being followed.
     */
    etl::optional<node_path_reversal_ok_t> path {etl::nullopt};
    /**
     * current node in path.
     */
    size_t i {}/*, j {}*/;
    /**
     * final destination. {} means not set.
     */
    position_t dest {};
    /**
     * distance required for braking.
     */
    int braking_dist {};
    /**
     * wait timer before reacceleration after reversing.
     */
    int reverse_timer {};

    enum state_t {
      // noop
      THINKING = 0,
      // when starting a new path segment, do acceleration
      ACCEL_TO_START_SEGMENT,
      RUNNING_SEGMENT,
      BREAKING,
      END_SEGMENT,
      WAITING_TO_REVERSE,
      // if we need to go to sensor but end up not being at the sensor, reaccelerate
      // fix it
      ACCEL_TO_FIX_SENSOR,
      FIXING_SENSOR,
    } state {};

    void reset() {
      path = etl::nullopt;
      i = 0;
      // j = 0;
      dest = {};
      braking_dist = 0;
      state = THINKING;
    }

    auto const &segment() const {
      return std::get<0>((*path)[i]);
    }

    /**
     * predict() will set this for us..
     */
    int &segment_dist_left() {
      return std::get<1>((*path)[i]);
    }

    void set_speed(int speed) {
      utils::enumed_class msg {
        tcmd::tc_msg_header::SPEED,
        tcmd::speed_cmd { train->num, speed },
      };
      train_courier->push(msg);
    }

    void set_switch(int sw, bool is_straight) {
      utils::enumed_class msg {
        sw,
        is_straight ? tcmd::switch_dir_t::S : tcmd::switch_dir_t::C,
      };
      switch_courier->push(msg);
    }

    void emergency_stop() {
      dest = {};
      path = etl::nullopt;
      state = THINKING;
      set_speed(0);
    }

    /**
     * do an action and advances state.
     */
    void perform();
  };

  /**
   * collection of information used to drive the traffic server.
   */
  struct traffic_controller {
    traffic_controller(train_courier_t *tcc, switch_courier_t *swc) {
      for (auto i : valid_trains()) {
        trains[i].num = {i};
        last_train_sensor_updates[i] = 0;
        drivers[i] = {&trains[i], &reserved_nodes, tcc, swc};
      }
      for (auto i : valid_switches()) {
        switches[i] = switch_dir_t::NONE;
      }
      init_reserved_nodes();
    }

    void init_reserved_nodes() {
      for (auto i : broken_switches()) {
        auto key = troll::sformat<5>("BR{}", i);
        auto *node = valid_nodes().at(key);
        reserved_nodes.insert(node);
        reserved_nodes.insert(node->reverse);
      }
    }

    void send_train_ui_msg(const internal_train_state &train) {
      auto &driver = drivers.at(train.num);
      utils::enumed_class msg {
        ui::display_msg_header::TRAIN_READ,
        ui::train_read {
          train.num,
          train.cmd,
          driver.dest,  // dest
          train.tick_snap.pos,  // curr pos
          train.tick_snap.speed,  // curr speed
          train.sn_delta_t,
          train.sn_delta_d,
        },
      };
      ui::out().send_value(msg);
    }

    void handle_speed_cmd(int current_tick, speed_cmd &cmd) {
      auto &train = trains.at(cmd.train);
      train.sn_accelerated = true;

      if (cmd.speed == train.cmd) {
        return;
      }
      handle_train_predict(train, current_tick);

      if (cmd.speed == 15) {
        train.lo_to_hi = false;
        if (train.sitting_on_sensor) {
          // sensor won't send a feedback for the reverse direction because it is pressed
          // so need to set the state here
          train.sensor_snap.pos.name = train.tick_snap.pos.name
            = valid_nodes().at(train.sensor_snap.pos.name)->reverse->name;
          train.sensor_snap.pos.offset = 0;
          train.tick_snap.pos.offset = -train.tick_snap.pos.offset;
        } else {
          train.approaching_sensor = !train.approaching_sensor;
        }
      } else if (train.cmd == 15) {
        train.lo_to_hi = true;
      } else {
        auto new_speed = train_speed(train.num, cmd.speed, train.cmd);
        auto old_speed = train.tick_snap.speed;
        train.lo_to_hi = new_speed >= old_speed;
        if (train.lo_to_hi) {
          train.tick_snap.accel = train_acceleration(cmd.train, cmd.speed, train.cmd);
        } else {
          train.tick_snap.accel = -train_acceleration(cmd.train, cmd.speed, train.cmd);
        }
      }

      train.cmd = cmd.speed;
      // package information and send to display controller
      send_train_ui_msg(train);
    }

    void handle_switch_cmd(switch_cmd &cmd) {
      switches.at(cmd.switch_num) = cmd.switch_dir;
      // forward to display controller
      utils::enumed_class msg {
        ui::display_msg_header::SWITCHES,
        ui::switch_read { cmd.switch_num, cmd.switch_dir },
      };
      ui::out().send_value(msg);
    }

    void handle_sensor_read(sensor_read &read) {
      for (auto *train_ptr : initialized_trains) {
        if (adjust_train_location_from_sensor(*train_ptr, read)) {
          send_train_ui_msg(*train_ptr);
        }
      }
      // forward to display controller
      utils::enumed_class msg {
        ui::display_msg_header::SENSOR_MSG,
        ui::sensor_read { read.sensor, read.tick },
      };
      ui::out().send_value(msg);
    }

    /**
     * for this method to work, we must ensure no other train tempers with the switch between
     * train's two sensor snaps, if there are sensors.
     */
    bool adjust_train_location_from_sensor(internal_train_state &train, sensor_read &read) {
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
        train.sn_accelerated = train.tick_snap.accel != fp{};
        train.approaching_sensor = false;
        train.sn_avg_speed = {};
      };

      // doing this to ensure: if train is stopped at sensor, then when it restarts, it will
      // have correct time tick. but if train is just passing by the sensor very slowly, just
      // ignore it.
      if (train_node == sensor_node) {
        last_train_sensor_updates[train.num] = read.tick;
        train.sitting_on_sensor = true;
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
        last_train_sensor_updates[train.num] = read.tick;
        bool train_was_on_sensor = train.sitting_on_sensor;
        train.sitting_on_sensor = true;
        // or it just reverses on the sensor, in which case sensor will be opposite.
        if (train_was_on_sensor) {
          train.sensor_snap.tick = train.tick_snap.tick = read.tick;
          return true;
        }
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

      last_train_sensor_updates[train.num] = read.tick;
      train.sitting_on_sensor = true;
      // need to report dd
      auto expected_d = fp(std::get<1>(*train_next));
      auto actual_d = fp(train.tick_snap.pos.offset);
      train.sn_delta_d = actual_d - expected_d;
      // make correction to driver's remaining distance
      auto &driver = drivers.at(train.num);
      if (driver.path) {
        driver.segment_dist_left() += int{train.sn_delta_d};
      }

      // report dt
      if (train.sn_avg_speed.value() != fp{}) {
        auto actual_t = fp(train.tick_snap.tick - train.sensor_snap.tick) / 100;
        auto expected_t = expected_d / train.sn_avg_speed.value();
        train.sn_delta_t = actual_t - expected_t;

        // make correction to train's speed, if there is no acceleration occurring
        if (!train.sn_accelerated && actual_t != fp{}) {
          const auto alpha = fp{0.7};
          auto &standard_v = train.lo_to_hi ? train_speed(train.num, train.cmd, 0) : train_speed(train.num, train.cmd, 14);
          auto new_v = alpha * train.tick_snap.speed + (fp{1} - alpha) * expected_d / actual_t;
          train.tick_snap.speed = standard_v = new_v;
        }
      }

      reset_snaps();
      return true;
    }

    void handle_train_predict(internal_train_state &train, int current_tick) {
      train.sitting_on_sensor = (current_tick - last_train_sensor_updates.at(train.num)) < train_sensor_expire_timeout;
      // short path
      if ((train.tick_snap.accel == fp{} && train.tick_snap.speed == fp{})) {
        train.tick_snap.tick = current_tick;
        train.sn_avg_speed.add({});
        return;
      }
      auto dt = fp(current_tick - train.tick_snap.tick) / 100;
      auto new_v = train.tick_snap.speed + train.tick_snap.accel * dt;

      // clamp speed to max values if acceleration is complete
      auto clamped = false;
      int ddist;
      if (train.tick_snap.accel != fp{}) {
        if (train.lo_to_hi) {
          auto target_v = train_speed(train.num, train.cmd, 0);
          if (new_v >= target_v) {
            new_v = target_v;
            clamped = true;
          }
        } else {
          auto target_v = train_speed(train.num, train.cmd, 14);
          if (new_v <= target_v) {
            new_v = target_v;
            clamped = true;
          }
        }
        // sloppy estimation: vf^2 - vi^0 = 2ad because our t is small
        ddist = int{(new_v * new_v - train.tick_snap.speed * train.tick_snap.speed) / (2 * train.tick_snap.accel)};
        // auto ddist = static_cast<int>(train.tick_snap.speed * dt + train.tick_snap.accel * dt * dt / 2);
      } else {
        ddist = int{new_v * dt};
      }
      if (train.approaching_sensor) {
        train.tick_snap.pos.offset -= ddist;
      } else {
        train.tick_snap.pos.offset += ddist;
      }
      if (clamped) {
        train.tick_snap.accel = {};
      }
      train.tick_snap.speed = new_v;
      train.tick_snap.tick = current_tick;
      train.sn_avg_speed.add(new_v);
      auto &driver = drivers.at(train.num);
      if (driver.path) {
        driver.segment_dist_left() -= ddist;
      }
      send_train_ui_msg(train);
    }

    /**
     * updates trains' locations and speeds based on estimations.
     */
    void handle_train_predict(int current_tick) {
      for (auto *train_ptr : initialized_trains) {
        handle_train_predict(*train_ptr, current_tick);
      }
    }

    void handle_train_driver() {
      for (auto *train_ptr : initialized_trains) {
        drivers.at(train_ptr->num).perform();
      }
    }

    void handle_train_pos_init(const train_pos_init_msg &msg) {
      auto &train = trains.at(msg.train);
      auto old_cmd = train.cmd;
      train = {};
      train.num = msg.train;
      train.cmd = old_cmd;
      train.sensor_snap.pos.name = train.tick_snap.pos.name = msg.name;
      train.tick_snap.pos.offset = msg.offset;
      if (std::find(initialized_trains.begin(), initialized_trains.end(), &train) == initialized_trains.end()) {
        initialized_trains.push_back(&train);
      }
      send_train_ui_msg(train);
    }

    void handle_train_deinit(const train_deinit_msg msg) {
      auto &train = trains.at(msg);
      auto **found = std::find(initialized_trains.begin(), initialized_trains.end(), &train);
      if (found == initialized_trains.end()) {
        ui::out().send_notice("Train is not initialized.");
        return;
      }
      auto old_cmd = train.cmd;
      train = {};
      train.num = msg;
      train.cmd = old_cmd;
      initialized_trains.erase(found);
      drivers.at(msg).reset();
      send_train_ui_msg(train);
    }

    void handle_train_pos_goto(const train_pos_goto_msg &msg) {
      auto &train = trains.at(msg.train);
      if (std::find(initialized_trains.begin(), initialized_trains.end(), &train) == initialized_trains.end()) {
        ui::out().send_notice("Train is not initialized.");
        return;
      } else if (drivers.at(msg.train).path) {
        ui::out().send_notice("Train already has a destination.");
        return;
      }

      const auto *from_node = valid_nodes().at(train.tick_snap.pos.name);
      const auto *to_node = valid_nodes().at(msg.name);
      auto &driver = drivers.at(msg.train);

      driver.path = find_path(from_node, to_node, train.tick_snap.pos.offset, msg.offset, reserved_nodes);
      if (!driver.path) {
        ui::out().send_notice(troll::sformat<128>(
          "Path finding for train {} was not possible: {} to {}. Routing failed.",
          msg.train,
          ui::stringify_pos(train.tick_snap.pos),
          ui::stringify_pos(msg)
        ));
        return;
      }

      ui::out().send_notice(troll::sformat<128>(
        "Path found for train {}: {} to {}. Length is {}.",
        msg.train,
        ui::stringify_pos(train.tick_snap.pos),
        ui::stringify_pos(msg),
        std::accumulate(driver.path->begin(), driver.path->end(), 0, [](int acc, auto &&seg) {
          return acc + std::get<1>(seg);
        })
      ));

      driver.i = /*driver.j =*/ 0;
      driver.state = mini_driver::ACCEL_TO_START_SEGMENT;
      driver.dest = { msg.name, msg.offset };
      send_train_ui_msg(train);
    }

    void handle_trains_stop() {
      for (auto &&[num, driver] : drivers) {
        driver.emergency_stop();
      }
      reserved_nodes.clear();
      init_reserved_nodes();
    }

    // information that is probably not good for credit if static

    /**
     * all possible train structures indexed by its num.
     */
    etl::unordered_map<int, internal_train_state, num_trains> trains {};
    /**
     * for each train, store its driving state.
     */
    etl::unordered_map<int, mini_driver, num_trains> drivers {};
    /**
     * all train numbers whose trains have been initialized.
     */
    etl::vector<internal_train_state *, num_trains> initialized_trains {};
    /**
     * timestamps when train activates sensor.
     */
    etl::unordered_map<int, int, num_trains> last_train_sensor_updates {};
    /**
     * switch statuses.
     */
    switch_status_t switches {};
    /**
     * nodes in reservation that cannot be routed to.
     */
    blocked_track_nodes_t reserved_nodes {};
  };

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

  void traffic_server() {
    RegisterAs(TRAFFIC_SERVER_TASK_NAME);
    auto clock_server = TaskFinder("clock_server");
    train_courier_t train_courier {
      priority_t::PRIORITY_L1, tcmd::TRAIN_TASK_NAME,
    };
    switch_courier_t switch_courier {
      priority_t::PRIORITY_L1, tcmd::SWITCH_TASK_NAME,
    };
    traffic_controller state {&train_courier, &switch_courier};

    utils::enumed_class<traffic_msg_header, char[128]> msg;
    tid_t request_tid;

    while (true) {
      train_courier.try_reply();
      switch_courier.try_reply();

      if (ReceiveValue(request_tid, msg) < 1) {
        continue;
      }
      switch (msg.header) {
      case traffic_msg_header::TO_TC_COURIER:
        train_courier.make_ready();
        break;
      case traffic_msg_header::TO_SWITCH_COURIER:
        switch_courier.make_ready();
        break;
      case traffic_msg_header::TRAIN_SPEED_CMD:
        state.handle_speed_cmd(Time(clock_server()), msg.data_as<speed_cmd>());
        ReplyValue(request_tid, null_reply);
        break;
      case traffic_msg_header::SWITCH_CMD:
        state.handle_switch_cmd(msg.data_as<switch_cmd>());
        ReplyValue(request_tid, null_reply);
        break;
      case traffic_msg_header::SENSOR_READ:
        state.handle_sensor_read(msg.data_as<sensor_read>());
        ReplyValue(request_tid, null_reply);
        break;
      case traffic_msg_header::TRAIN_PREDICT:
        state.handle_train_predict(Time(clock_server()));
        state.handle_train_driver();
        ReplyValue(request_tid, null_reply);
        break;
      case traffic_msg_header::TRAIN_POS_INIT:
        state.handle_train_pos_init(msg.data_as<train_pos_init_msg>());
        ReplyValue(request_tid, traffic_reply_msg::OK);
        break;
      case traffic_msg_header::TRAIN_POS_DEINIT:
        state.handle_train_deinit(msg.data_as<train_deinit_msg>());
        ReplyValue(request_tid, traffic_reply_msg::OK);
        break;
      case traffic_msg_header::TRAIN_POS_GOTO:
        state.handle_train_pos_goto(msg.data_as<train_pos_goto_msg>());
        ReplyValue(request_tid, traffic_reply_msg::OK);
        break;
      case traffic_msg_header::TRAINS_STOP:
        state.handle_trains_stop();
        ReplyValue(request_tid, traffic_reply_msg::OK);
        break;
      default:
        break;
      }
    }
  }

  void predict_timer() {
    auto traffic_server = TaskFinder(TRAFFIC_SERVER_TASK_NAME);
    auto clock_server = TaskFinder("clock_server");
    while (true) {
      Delay(clock_server(), predict_react_interval);
      SendValue(traffic_server(), traffic_msg_header::TRAIN_PREDICT, null_reply);
    }
  }

  void init_tasks() {
    Create(priority_t::PRIORITY_L1, predict_timer);
    Create(priority_t::PRIORITY_L1, traffic_server);
  }

} // namespace traffic
