#include "trains.hpp"
#include "kern/merklin.hpp"
#include "kern/user_syscall_typed.hpp"
#include "ui.hpp"
#include "generic/utils.hpp"

namespace trains {

void switch_expire_timer() {
  tid_t switch_task = MyParentTid();
  auto clock_server = TaskFinder("clock_server");
  while (true) {
    SendValue(switch_task, switch_cmd {}/*dummy*/, null_reply);
    DelayUntil(clock_server(), Time(clock_server()) + 30);  // 300ms
  }
}

void switch_task() {
  RegisterAs(SWITCH_TASK_NAME);
  tid_t train_controller = MyParentTid();
  tid_t expire_timer = Create(priority_t::PRIORITY_L2, switch_expire_timer);

  tid_t request_tid;
  utils::enumed_class<tc_msg_header, switch_cmd> buffer;
  buffer.header = tc_msg_header::SWITCH_CMD_PART_1;
  int turn_off_counts = 0;

  while (1) {
    if (ReceiveValue(request_tid, buffer.data) <= 0) {
      continue;
    }
    if (request_tid == expire_timer) {
      if (turn_off_counts && turn_off_counts - 1 == 0) {
        --turn_off_counts;
        SendValue(train_controller, tc_msg_header::SWITCH_CMD_PART_2, null_reply);
      } else if (turn_off_counts) {
        ReplyValue(request_tid, tc_reply::OK);
      }
    } else {
      tc_reply reply;
      int replylen = SendValue(train_controller, buffer, reply);
      ReplyValue(request_tid, tc_reply::OK);
      if (replylen == 1 && reply == tc_reply::OK) {
        ++turn_off_counts;
        ReplyValue(expire_timer, tc_reply::OK);
      }
    }
  }
}

void reverse_task() {
  RegisterAs(REVERSE_TASK_NAME);
  auto clock_server = TaskFinder("clock_server");
  tid_t train_controller = MyParentTid();

  tid_t request_tid;
  utils::enumed_class<tc_msg_header, reverse_cmd> buffer;

  while (1) {
    int request = ReceiveValue(request_tid, buffer.data);
    if (request <= 0) continue;
    ReplyValue(request_tid, tc_reply::OK);

    buffer.header = tc_msg_header::REVERSE_CMD_PART_1;

    tc_reply reply;
    int replylen = SendValue(train_controller, buffer, reply);

    // need to think about what really happens if we send multiple reverse commands
    if (replylen == 1 && reply == tc_reply::TRAIN_ALREADY_REVERSING) {
    } else {
      DelayUntil(clock_server(), Time(clock_server()) + 300); // 3 seconds or 3000ms
      buffer.header = tc_msg_header::REVERSE_CMD_PART_2;
      SendValue(train_controller, buffer, null_reply);
      DelayUntil(clock_server(), Time(clock_server()) + 100);
      buffer.header = tc_msg_header::REVERSE_CMD_PART_3;
      SendValue(train_controller, buffer, null_reply);
    }
  }
}

void sensor_task() {
  RegisterAs(SENSOR_TASK_NAME);
  auto track_task = TaskFinder(tracks::TRACK_SERVER_TASK_NAME);
  auto clock_server = TaskFinder("clock_server");
  tid_t train_controller = MyParentTid();

  auto send_sensor = [&track_task/*, &train_controller*/] (int tick, char module_char, int offset) {
    utils::enumed_class<tracks::track_msg_header, tracks::sensor_read> sensor_msg;
    sensor_msg.header = tracks::track_msg_header::SENSOR_READ;
    sensor_msg.data.sensor[0] = module_char;
    if (offset < 10) {
      sensor_msg.data.sensor[1] = '0' + offset;
      sensor_msg.data.sensor[2] = '\0';
      } else {
      sensor_msg.data.sensor[1] = '0' + offset / 10;
      sensor_msg.data.sensor[2] = '0' + offset % 10;
      sensor_msg.data.sensor[3] = '\0';
    }
    sensor_msg.data.tick = tick;
    // stop distance experiment
    /*if (sensor_msg.data.sensor == "E12") {
      SendValue(train_controller, utils::enumed_class {
        tc_msg_header::SPEED,
        speed_cmd { 1, 0 }
      }, null_reply);
      ui::send_notice("automatically sent stop command");
    }*/
    SendValue(track_task(), sensor_msg, null_reply);
  };

  char sensor_bytes[10] = {0};

  while (1) {
    int replylen = SendValue(train_controller, tc_msg_header::SENSOR_CMD, sensor_bytes);
    int tick = Time(clock_server());
    if (replylen == 10) {
      char contact_1_8, contact_9_16;
      size_t idx = 0;
      char module_char = 'A';
      size_t i;
      while (idx < 10) {
        contact_1_8 = sensor_bytes[idx++];
        contact_9_16 = sensor_bytes[idx++];

        for (i = 0; i < 8; ++i) {
          if (((contact_1_8 >> i) & 1) == 1) {
            send_sensor(tick, module_char, 8 - i);
          }
        }

        for (i = 0; i < 8; ++i) {
          if (((contact_9_16 >> i) & 1) == 1) {
            send_sensor(tick, module_char, 16 - i);
          }
        }
        ++module_char;
      }
    }

    Delay(clock_server(), 6); // 60ms
  }
}

void train_controller_task() {
  RegisterAs(TRAIN_CONTROLLER_NAME);
  auto merklin_tx = TaskFinder(merklin::MERK_TX_SERVER_NAME);
  auto merklin_rx = TaskFinder(merklin::MERK_RX_SERVER_NAME);
  auto track_task = TaskFinder(tracks::TRACK_SERVER_TASK_NAME);

#if DEBUG_PI == 0
  Create(PRIORITY_L2, sensor_task);
#endif
  Create(PRIORITY_L2, switch_task);
  Create(PRIORITY_L2, reverse_task);

  tid_t request_tid;
  utils::enumed_class<tc_msg_header, char[128]> message {};

  // TODO: get rid of this
  int train_speeds[81];
  bool is_train_reversing[81];

  for (size_t i = 0; i < 81; ++i) {
    train_speeds[i] = 0;
    is_train_reversing[i] = false;
  }

  switch_dir_t switch_directions[256];
  for (size_t i = 0; i < 256; ++i) {
    switch_directions[i] = switch_dir_t::NONE;
  }

  auto send_train_speed = [&train_speeds, &merklin_tx, &track_task, &request_tid] (int train_num, int speed) {
    Putc(merklin_tx(), 1, (char)speed);
    Putc(merklin_tx(), 1, (char)train_num);

    // track task may send cmds back to me so unblock it first
    ReplyValue(request_tid, tc_reply::OK);

    utils::enumed_class track_msg {
      tracks::track_msg_header::TRAIN_SPEED_CMD,
      tracks::speed_cmd { train_num, speed },
    };
    SendValue(track_task(), track_msg, null_reply);
  };

  while (1) {
    int request = ReceiveValue(request_tid, message);
    if (request <= 0) continue;

    switch (message.header) {
      case tc_msg_header::REVERSE_CMD_PART_1: {
        auto &cmd = message.data_as<reverse_cmd>();
        int train_num = cmd.train;

        if (!is_train_reversing[train_num]) {
          send_train_speed(train_num, train_speeds[train_num] >= 16 ? 16 : 0);
          is_train_reversing[train_num] = true;
        } else {
          ReplyValue(request_tid, tc_reply::TRAIN_ALREADY_REVERSING);
        }
        break;
      }
      case tc_msg_header::REVERSE_CMD_PART_2: {
        auto &cmd = message.data_as<reverse_cmd>();
        int train_num = cmd.train;
        send_train_speed(train_num, 15);
        break;
      }
      case tc_msg_header::REVERSE_CMD_PART_3: {
        auto &cmd = message.data_as<reverse_cmd>();
        int train_num = cmd.train;

        send_train_speed(train_num, train_speeds[train_num]);
        is_train_reversing[train_num] = false;
        break;
      }
      case tc_msg_header::SENSOR_CMD: {
        char sensor_bytes[10];
        Putc(merklin_tx(), 1, static_cast<char>(special_cmd::READ_SENSORS));
        for (int i = 0; i < 10; ++i) {
          sensor_bytes[i] = Getc(merklin_rx(), 1);
        }
        ReplyValue(request_tid, sensor_bytes);
        break;
      }
      case tc_msg_header::SWITCH_CMD_PART_1: {
        auto &cmd = message.data_as<switch_cmd>();
        if (switch_directions[cmd.switch_num] == cmd.switch_dir) {
          ReplyValue(request_tid, tc_reply::SWITCH_UNCHANGED);
        } else {
          Putc(merklin_tx(), 1, (char)(cmd.switch_dir));
          Putc(merklin_tx(), 1, (char)(cmd.switch_num));
          switch_directions[cmd.switch_num] = cmd.switch_dir;
          ReplyValue(request_tid, tc_reply::OK);
          //
          SendValue(track_task(), utils::enumed_class {
            tracks::track_msg_header::SWITCH_CMD,
            tracks::switch_cmd { cmd.switch_num, cmd.switch_dir },
          }, null_reply);

        }
        break;
      }
      case tc_msg_header::SWITCH_CMD_PART_2: {
        Putc(merklin_tx(), 1, static_cast<char>(special_cmd::TURNOFF_SWITCH));
        ReplyValue(request_tid, tc_reply::OK);
        break;
      }
      case tc_msg_header::SPEED: {
        auto &cmd = message.data_as<speed_cmd>();
        int speed = cmd.speed;
        int train_num = cmd.train;

        if (!is_train_reversing[train_num]) {
          train_speeds[train_num] = speed;
          send_train_speed(train_num, speed);
        } else {
          ReplyValue(request_tid, tc_reply::TRAIN_ALREADY_REVERSING);
        }
        break;
      }
      case tc_msg_header::GO_CMD: {
        Putc(merklin_tx(), 1, static_cast<char>(special_cmd::GO));
        ReplyValue(request_tid, tc_reply::OK);
        break;
      }
      case tc_msg_header::SET_RESET_MODE: {
        Putc(merklin_tx(), 1, static_cast<char>(special_cmd::RESET_MODE));
        ReplyValue(request_tid, tc_reply::OK);
        break;
      }
      default: break;
    }
  }
}

void init_tasks() {
  Create(PRIORITY_L1, train_controller_task);
}

}
