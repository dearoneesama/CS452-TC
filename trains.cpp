#include "trains.hpp"
#include "merklin.hpp"
#include "user_syscall.h"
#include "ui.hpp"

namespace trains {

void switch_task() {
  RegisterAs(SWITCH_TASK_NAME);
  tid_t clock_server = WhoIs("clock_server");
  tid_t train_controller = MyParentTid();

  tid_t request_tid;
  constexpr size_t buffer_size = 1 + sizeof(switch_cmd);
  char buffer[buffer_size] = {0};
  buffer[0] = static_cast<char>(tc_msg_header::SWITCH_CMD_PART_1);
  char turnoff_msg = static_cast<char>(tc_msg_header::SWITCH_CMD_PART_2);
  switch_cmd *msg = (switch_cmd*)(buffer+1);
  char reply;

  while (1) {
    int request = Receive((int*)&request_tid, (char*)msg, sizeof(switch_cmd));
    if (request <= 0) continue;
    int replylen = Send(train_controller, buffer, buffer_size, &reply, 1);

    // if the switch was already at that position
    if (replylen == 1 && reply == static_cast<char>(tc_reply::SWITCH_UNCHANGED)) {
      Reply(request_tid, &reply, 1);
    } else {
      Delay(clock_server, 20); // 200ms
      Send(train_controller, &turnoff_msg, 1, &reply, 1);
      Reply(request_tid, &reply, 1);
    }

  }
}

void reverse_task() {
  RegisterAs(REVERSE_TASK_NAME);
  tid_t clock_server = WhoIs("clock_server");
  tid_t train_controller = MyParentTid();

  tid_t request_tid;
  constexpr size_t buffer_size = 1 + sizeof(reverse_cmd);
  char buffer[buffer_size] = {0};
  
  reverse_cmd *msg = (reverse_cmd*)(buffer+1);
  char reply;

  while (1) {
    int request = Receive((int*)&request_tid, (char*)msg, sizeof(reverse_cmd));
    if (request <= 0) continue;

    buffer[0] = static_cast<char>(tc_msg_header::REVERSE_CMD_PART_1);
    int replylen = Send(train_controller, buffer, buffer_size, &reply, 1);

    // need to think about what really happens if we send multiple reverse commands
    if (replylen == 1 && reply == static_cast<char>(tc_reply::TRAIN_ALREADY_REVERSING)) {
      Reply(request_tid, &reply, 1);
    } else {
      Delay(clock_server, 300); // 3 seconds or 3000ms
      buffer[0] = static_cast<char>(tc_msg_header::REVERSE_CMD_PART_2);
      Send(train_controller, buffer, buffer_size, &reply, 1);
      Reply(request_tid, &reply, 1);
    }
  }
}

class circular_buffer {
public:
  void add(char c) {
    buffer_[next_index_] = c;
    next_index_ = (next_index_ + 1) & (127);
    if (size_ != 128) {
      ++size_;
    }
  }

  size_t size() const {
    return size_;
  }

  // requires 0 <= i < 128
  char most_recent_at(size_t i) const {
    if (size_ == 128) {
      return buffer_[(next_index_ + size_ - 1 - i) & (127)];
    }
    return buffer_[next_index_ - 1 - i];
  }

private:
  char buffer_[128];
  size_t next_index_ = 0;
  size_t size_ = 0;
};

class sensor_buffer_t {
public:
  void add(char module, char contact) {
    modules_.add(module);
    contacts_.add(contact);
  }

  size_t fill_message(char *message) {
    size_t num_sensors = modules_.size() > 10 ? 10 : modules_.size();
    for (size_t i = 0; i < num_sensors; ++i) {
      char sensor_module = modules_.most_recent_at(i);
      char sensor_contact = contacts_.most_recent_at(i);
      *(message++) = sensor_module;
      *(message++) = '.';
      *(message++) = '0' + (sensor_contact >= 10 ? 1 : 0);
      *(message++) = '0' + (sensor_contact % 10);
    }
    return 4 * num_sensors;
  }

private:
  circular_buffer modules_;
  circular_buffer contacts_;
};

void sensor_task() {
  RegisterAs(SENSOR_TASK_NAME);
  tid_t clock_server = WhoIs("clock_server");
  tid_t train_controller = MyParentTid();
  tid_t display_controller = WhoIs(ui::DISPLAY_CONTROLLER_NAME);
  sensor_buffer_t sensor_buffer{};

  char sensor_bytes[10] = {0};
  char cmd = static_cast<char>(tc_msg_header::SENSOR_CMD);
  // size_t num_sensors = 5 * 16;
  char sensor_line_output[1 + 40];
  sensor_line_output[0] = static_cast<char>(ui::display_msg_header::SENSOR_MSG);
  char reply;

  while (1) {
    int replylen = Send(train_controller, &cmd, 1, sensor_bytes, 10);
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
            sensor_buffer.add(module_char, 8 - i);
          }
        }

        for (i = 0; i < 8; ++i) {
          if (((contact_9_16 >> i) & 1) == 1) {
            sensor_buffer.add(module_char, 16 - i);
          }
        }
        ++module_char;
      }

      size_t msg_len = 1 + sensor_buffer.fill_message(sensor_line_output+1);
      Send(display_controller, sensor_line_output, msg_len, &reply, 1);
    }

    Delay(clock_server, 20); // 200ms
  }
}

void train_controller_task() {
  RegisterAs(TRAIN_CONTROLLER_NAME);
  tid_t merklin_tx = WhoIs(merklin::MERK_TX_SERVER_NAME);
  tid_t merklin_rx = WhoIs(merklin::MERK_RX_SERVER_NAME);
  Create(PRIORITY_L1, sensor_task);
  Create(PRIORITY_L1, switch_task);
  Create(PRIORITY_L1, reverse_task);

  tid_t request_tid;
  char message[128];

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

  char reply;
  char sensor_bytes[10];

  while (1) {
    int request = Receive((int*)&request_tid, message, 128);
    if (request <= 0) continue;

    switch (static_cast<tc_msg_header>(message[0])) {
      case tc_msg_header::REVERSE_CMD_PART_1: {
        reverse_cmd* cmd = (reverse_cmd*)(message+1);
        int train_num = cmd->train;

        if (!is_train_reversing[train_num]) {
          Putc(merklin_tx, 1, train_speeds[train_num] >= 16 ? 16 : 0);
          Putc(merklin_tx, 1, (char)train_num);
          reply = static_cast<char>(tc_reply::OK);
          Reply(request_tid, &reply, 1);
          is_train_reversing[train_num] = true;
        } else {
          reply = static_cast<char>(tc_reply::TRAIN_ALREADY_REVERSING);
          Reply(request_tid, &reply, 1);
        }
        break;
      }
      case tc_msg_header::REVERSE_CMD_PART_2: {
        reverse_cmd* cmd = (reverse_cmd*)(message+1);
        int train_num = cmd->train;

        Putc(merklin_tx, 1, 15);
        Putc(merklin_tx, 1, (char)train_num);

        Putc(merklin_tx, 1, train_speeds[train_num]);
        Putc(merklin_tx, 1, (char)train_num);

        is_train_reversing[train_num] = false;
        reply = static_cast<char>(tc_reply::OK);
        Reply(request_tid, &reply, 1);
        break;
      }
      case tc_msg_header::SENSOR_CMD: {
        Putc(merklin_tx, 1, static_cast<char>(special_cmd::READ_SENSORS));
        for (int i = 0; i < 10; ++i) {
          sensor_bytes[i] = Getc(merklin_rx, 1);
        }
        Reply(request_tid, sensor_bytes, 10);
        break;
      }
      case tc_msg_header::SWITCH_CMD_PART_1: {
        switch_cmd* cmd = (switch_cmd*)(message+1);
        if (switch_directions[cmd->switch_num] == cmd->switch_dir) {
          reply = static_cast<char>(tc_reply::SWITCH_UNCHANGED);
          Reply(request_tid, &reply, 1);
        } else {
          Putc(merklin_tx, 1, (char)(cmd->switch_dir));
          Putc(merklin_tx, 1, (char)(cmd->switch_num));
          switch_directions[cmd->switch_num] = cmd->switch_dir;
          reply = static_cast<char>(tc_reply::OK);
          Reply(request_tid, &reply, 1);
        }
        break;
      }
      case tc_msg_header::SWITCH_CMD_PART_2: {
        Putc(merklin_tx, 1, static_cast<char>(special_cmd::TURNOFF_SWITCH));
        break;
      }
      case tc_msg_header::SPEED: {
        speed_cmd* cmd = (speed_cmd*)(message+1);
        int speed = cmd->speed;
        int train_num = cmd->train;

        if (!is_train_reversing[train_num]) {
          Putc(merklin_tx, 1, (char)speed);
          Putc(merklin_tx, 1, (char)train_num);
          train_speeds[train_num] = speed;
          reply = static_cast<char>(tc_reply::OK);
          Reply(request_tid, &reply, 1);
        } else {
          reply = static_cast<char>(tc_reply::TRAIN_ALREADY_REVERSING);
          Reply(request_tid, &reply, 1);
        }
        break;
      }
      case tc_msg_header::GO_CMD: {
        Putc(merklin_tx, 1, static_cast<char>(special_cmd::GO));
        reply = static_cast<char>(tc_reply::OK);
        Reply(request_tid, &reply, 1);
        break;
      }
      case tc_msg_header::SET_RESET_MODE: {
        Putc(merklin_tx, 1, static_cast<char>(special_cmd::RESET_MODE));
        reply = static_cast<char>(tc_reply::OK);
        Reply(request_tid, &reply, 1);
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
