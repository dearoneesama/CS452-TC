#include "tracks.hpp"
#include "user_syscall_typed.hpp"
#include "ui.hpp"

namespace tracks {

  etl::array<int, num_switches> const &valid_switches() {
    static etl::array<int, num_switches> valid_switches = {
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
      11, 12, 13, 14, 15, 16, 17, 18,
      153, 154, 155, 156,
    };
    return valid_switches;
  }

  etl::array<int, num_trains> const &valid_trains() {
    static etl::array<int, num_trains> valid_trains = {
      1, 2, 24, 58, 74, 78,
    };
    return valid_trains;
  }

  troll::string_map<const track_node *, 4, TRACK_MAX> const &valid_nodes() {
    static bool initialized = false;
    static track_node raw_nodes[TRACK_MAX];
    static troll::string_map<const track_node *, 4, TRACK_MAX> valid_nodes;

    if (!initialized) {
#if IS_TRACK_A == 1
      init_tracka(raw_nodes);
#else
      init_trackb(raw_nodes);
#endif
      for (size_t i = 0; i < TRACK_MAX; ++i) {
        valid_nodes[raw_nodes[i].name] = &raw_nodes[i];
      }
      initialized = true;
    }

    return valid_nodes;
  }

  /**
   * information about a train.
   */
  struct internal_train_state {
    // train number
    int num {};
    // current speed command
    int cmd {};

    struct snapshot_t {
      position_t pos {};
      unsigned speed {};
      unsigned accel {};
      unsigned tick {};
    };

    // information based on last sensor reading
    snapshot_t sensor_snap {};
    // information based on last tick
    // the two fields will be in sync one train hits a sensor
    snapshot_t tick_snap {};
  };

  /**
   * collection of information used to drive the track server.
   */
  struct track_state {
    track_state() {
      for (auto i : valid_trains()) {
        trains[i] = {i};
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
        static_cast<int>(train.tick_snap.speed),  // curr speed
        0,
        0,
      };
    }

    void handle_speed_cmd(speed_cmd &cmd, tid_t display_controller) {
      auto &train = trains.at(cmd.train);
      train.cmd = cmd.speed;
      // package information and send to display controller
      utils::enumed_class msg {
        ui::display_msg_header::TRAIN_READ,
        make_train_ui_msg(train),
      };
      SendValue(display_controller, msg, null_reply);
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
      // forward to display controller
      utils::enumed_class msg {
        ui::display_msg_header::SENSOR_MSG,
        ui::sensor_read { read.sensor, read.tick },
      };
      SendValue(display_controller, msg, null_reply);
    }

    // information that is probably not good for credit if static
    etl::unordered_map<int, internal_train_state, num_trains> trains {};
    etl::unordered_map<int, switch_dir_t, num_switches> switches {};
  };

  void track_server() {
    track_state state;
    RegisterAs(TRACK_SERVER_TASK_NAME);
    auto display_controller = TaskFinder(ui::DISPLAY_CONTROLLER_NAME);

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
      default:
        break;
      }
    }
  }

  void init_tasks() {
    Create(priority_t::PRIORITY_L1, track_server);
  }

} // namespace tracks
