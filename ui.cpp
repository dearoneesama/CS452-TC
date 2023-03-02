#include "ui.hpp"
#include "gtkterm.hpp"
#include "user_syscall.h"
#include "kstddefs.hpp"
#include "format.hpp"
#include "trains.hpp"
#include "format_scan.hpp"
#include "rpi.hpp"

namespace ui {
const size_t num_solenoids = 22;

const char active_solenoids[] = {
     1,    2,  3,  4,  5,  6,  7,  8,    9,   10,
    11,   12, 13, 14, 15, 16, 17, 18, 0x99, 0x9A, // 153, 154
  0x9B, 0x9C }; // 155, 156

void display_controller_task() {
  RegisterAs(DISPLAY_CONTROLLER_NAME);
  tid_t gtkterm_tx = WhoIs(gtkterm::GTK_TX_SERVER_NAME);

  tid_t request_tid;
  const char* idle_format = "\0337\033[1;1H\033[KKernel: {}%  User: {}%  Idle: {}%\0338";
  const char* timer_format = "\0337\033[2;1H\033[KTime: {}:{}:{}:{}\0338";
  const char* sensor_format = "\0337\033[3;1H\033[KSensors: {}\0338";
  const char* move_cursor_write_format = "\0337\033[{}:{}H\033[K{}\0338";

  const int offset_col = 16;
  const int offset_per_index = 7;
  const int offset_row = 4;

  // xxx: (s|c|?)
  const char* init_switch_table = "\0337\033[4;1J\033[KSwitches:   1: ?   2: ?   3: ?   4: ?   5: ?   6: ?   7: ?   8: ?   9: ?  10: ?" \
                                           "\r\n           11: ?  12: ?  13: ?  14: ?  15: ?  16: ?  17: ?  18: ? 153: ? 154: ?" \
                                           "\r\n          155: ? 156: ?\0338";

  for (size_t i = 0; i < 198; ++i) {
    Putc(gtkterm_tx, 0, init_switch_table[i]);
  }

  char buffer[128];
  char message[128];
  const char* reply = "o";

  while (1) {
    int request = Receive((int*)&request_tid, message, 128);

    switch (static_cast<display_msg_header>(message[0])) {
      case display_msg_header::IDLE_MSG : { // idle
        time_percentage_t* percentages = (time_percentage_t*) (message+1);
        auto len = troll::snformat(buffer, idle_format,
          percentages->kernel,
          percentages->user,
          percentages->idle);
        for (size_t i = 0; i < len; ++i) {
          Putc(gtkterm_tx, 0, buffer[i]);
        }
        Reply(request_tid, reply, 1);
        break;
      }
      case display_msg_header::USER_INPUT: { // user input
        Putc(gtkterm_tx, 0, message[1]);
        Reply(request_tid, reply, 1);
        break;
      }
      case display_msg_header::STRING: { // string
        for (int i = 1; i < request; ++i) {
          Putc(gtkterm_tx, 0, message[i]);
        }
        Reply(request_tid, reply, 1);
        break;
      }
      case display_msg_header::TIMER_CLOCK_MSG: { // time
        timer_clock_t* time = (timer_clock_t*)(message + 1);
        auto len = troll::snformat(buffer, timer_format,
          time->hours,
          time->minutes,
          time->seconds,
          time->hundred_ms);
        for (size_t i = 0; i < len; ++i) {
          Putc(gtkterm_tx, 0, buffer[i]);
        }
        Reply(request_tid, reply, 1);
        break;
      }
      case display_msg_header::SENSOR_MSG: { // sensors
        message[request] = '\0';
        auto len = troll::snformat(buffer, sensor_format, message + 1);
        for (size_t i = 0; i < len; ++i) {
          Putc(gtkterm_tx, 0, buffer[i]);
        }
        Reply(request_tid, reply, 1);
        break;
      }
      case display_msg_header::SWITCHES: { // we assume that only changing active switches will go through here
        trains::switch_cmd* cmd = (trains::switch_cmd*) (message+1);
        char dir = cmd->switch_dir == trains::switch_dir_t::C ? 'C' : 'S';
        int switch_num = cmd->switch_num;
        int row, col;
        if (switch_num <= 10) {
          // first row
          row = offset_row;
          col = offset_col + (switch_num - 1) * offset_per_index;
        } else if (switch_num <= 154) {
          // second row
          row = offset_row + 1;
          if (switch_num == 153 || switch_num == 154) {
            col = offset_col + (switch_num - 153 + 8) * offset_per_index;
          } else {
            col = offset_col + (switch_num - 11) * offset_per_index;
          }
        } else {
          // last row
          row = offset_row + 2;
          col = offset_col + (switch_num - 155) * offset_per_index;
        }
        auto len = troll::snformat(buffer, move_cursor_write_format, row, col, dir);
        for (size_t i = 0; i < len; ++i) {
          Putc(gtkterm_tx, 0, buffer[i]);
        }
        Reply(request_tid, reply, 1);
        break;
      }
      default: {
        break;
      }
    }
  }
}

void command_controller_task() {
  tid_t gtkterm_rx = WhoIs(gtkterm::GTK_RX_SERVER_NAME);
  tid_t display_controller = WhoIs(DISPLAY_CONTROLLER_NAME);
  tid_t train_controller = WhoIs(trains::TRAIN_CONTROLLER_NAME);
  tid_t reverse_task = WhoIs(trains::REVERSE_TASK_NAME);
  tid_t switch_task = WhoIs(trains::SWITCH_TASK_NAME);

  char command_buffer[1 + 64];
  command_buffer[0] = static_cast<char>(display_msg_header::STRING);

  size_t curr_size = 1;
  char input_msg[2];
  input_msg[0] = static_cast<char>(display_msg_header::USER_INPUT);
  char reply;

  const char *history = "s\033[49;1H\033[K> ";
  const char *clear_line = "s\033[50;1H\033[K> ";
  const char *back_space = "s\033[1D\033[K";
  Send(display_controller, clear_line, 13, &reply, 1);

  auto is_valid_train = [](int arg) {
    return 1 <= arg && arg <= 80;
  };
  auto is_valid_speed = [](int arg) {
    return (0 <= arg && arg <= 14) || (16 <= arg && arg <= 30);
  };
  auto is_valid_solenoid = [](int arg) {
    return 0 <= arg && arg <= 255;
  };
  auto is_valid_solenoid_dir = [](int arg) {
    return arg == 'S' || arg == 'C';
  };

  char speed_cmd_msg[1 + sizeof(trains::speed_cmd)];
  speed_cmd_msg[0] = static_cast<char>(trains::tc_msg_header::SPEED);

  trains::speed_cmd *scmd = (trains::speed_cmd*)(speed_cmd_msg+1);
  trains::reverse_cmd rcmd = {0};

  char switch_cmd_msg[1 + sizeof(trains::switch_cmd)];
  switch_cmd_msg[0] = static_cast<char>(display_msg_header::SWITCHES);
  trains::switch_cmd *swcmd = (trains::switch_cmd*)(switch_cmd_msg+1);
  swcmd->switch_dir = trains::switch_dir_t::NONE;
  swcmd->switch_num = 0;

  while (1) {
    char c = Getc(gtkterm_rx, 0);
    if (c == '\r') {
      Send(display_controller, history, 13, &reply, 1);
      Send(display_controller, command_buffer, curr_size, &reply, 1);

      int arg1 = 0, arg2 = 0;
      char char_arg = 0;
      bool valid = false;
      /**
       * Formats:
       *   tr [1-80] [0-14 or 16-30]
       *   rv [1-80]
       *   sw [0-255] (S|C)
       */
      if (troll::sscan(command_buffer + 1, curr_size - 1, "tr {} {}", arg1, arg2)) {
        if (is_valid_train(arg1) && is_valid_speed(arg2)) {
          scmd->train = arg1;
          scmd->speed = arg2;
          int replylen = Send(train_controller, speed_cmd_msg, 1 + sizeof(trains::speed_cmd), &reply, 1);
          if (replylen == 1 && reply == static_cast<char>(trains::tc_reply::OK)) {
            valid = true;
          }
        }
      } else if (troll::sscan(command_buffer + 1, curr_size - 1, "rv {}", arg1)) {
        if (is_valid_train(arg1)) {
          rcmd.train = arg1;
          int replylen = Send(reverse_task, (char*)&rcmd, sizeof(trains::reverse_cmd), &reply, 1);
          if (replylen == 1 && reply == static_cast<char>(trains::tc_reply::OK)) {
            valid = true;
          }
        }
      } else if (troll::sscan(command_buffer + 1, curr_size - 1, "sw {} {}", arg1, char_arg)) {
        if (is_valid_solenoid(arg1) && is_valid_solenoid_dir(char_arg)) {
          // filter out the solenoids that are not even on the track
          for (size_t i = 0; i < num_solenoids; ++i) {
            if (arg1 == active_solenoids[i]) {
              swcmd->switch_num = arg1;
              swcmd->switch_dir = arg2 == 'S' ? trains::switch_dir_t::S : trains::switch_dir_t::C;
              int replylen = Send(switch_task, (char*)swcmd, sizeof(trains::switch_cmd), &reply, 1);
              if (replylen == 1 && reply == static_cast<char>(trains::tc_reply::OK)) {
                Send(display_controller, switch_cmd_msg, 1 + sizeof(trains::switch_cmd), &reply, 1);
                valid = true;
              }
              break;
            }
          }
        }
      } else if (curr_size == 2 && command_buffer[1] == 'q') {
        Terminate();
      }

      if (!valid) {
        Send(display_controller, "s Invalid!", 10, &reply, 1);
      }
      Send(display_controller, clear_line, 13, &reply, 1);
      curr_size = 1;
    } else if (c == 8 && curr_size > 1) { // back space
      curr_size--;
      Send(display_controller, back_space, 8, &reply, 1);
    } else if (curr_size < 65) {
      command_buffer[curr_size++] = c;
      input_msg[1] = c;
      Send(display_controller, input_msg, 2, &reply, 1);
    }
  }
}

void timer_task() {
  tid_t display_controller = WhoIs(DISPLAY_CONTROLLER_NAME);
  tid_t clock_server = WhoIs("clock_server");

  char message[1 + sizeof(timer_clock_t)];
  message[0] = display_msg_header::TIMER_CLOCK_MSG;
  timer_clock_t *time = (timer_clock_t*)(message+1);
  time->hours = 0;
  time->minutes = 0;
  time->seconds = 0;
  time->hundred_ms = 0;
  char reply;

  uint32_t current_time = Time(clock_server);

  while (1) {
    current_time = DelayUntil(clock_server, current_time + 10); // every 100ms
    time->hundred_ms++;
    if (time->hundred_ms == 10) {
      time->seconds++;
      time->hundred_ms = 0;
    }
    if (time->seconds == 60) {
      time->minutes++;
      time->seconds = 0;
    }
    if (time->minutes == 60) {
      time->hours++;
      time->minutes = 0;
    }
    Send(display_controller, message, 1 + sizeof(timer_clock_t), &reply, 1);
  }
}

// todo: move this somewhere else
void idle_task() {
  tid_t display_ctrl = WhoIs(DISPLAY_CONTROLLER_NAME);

  time_percentage_t prev_percentages = {0, 0, 0};
  time_percentage_t curr_percentages = {0, 0, 0};
  time_distribution_t td = {0, 0, 0};
  constexpr size_t messagelen = 1 + sizeof(time_percentage_t);
  char message[messagelen];
  message[0] = static_cast<char>(display_msg_header::IDLE_MSG);
  size_t entries = 10;

  char reply;
  while (1) {
    --entries;
    // only print every 10 entries to the idle task to save the planet even more
    if (entries == 0) {
      entries = 10;
      TimeDistribution(&td);
      curr_percentages.kernel = (td.kernel_ticks * 100) / td.total_ticks;
      curr_percentages.user = ((td.total_ticks - td.kernel_ticks - td.idle_ticks) * 100) / td.total_ticks;
      curr_percentages.idle = (td.idle_ticks * 100) / td.total_ticks;

      // only print if the percentages are different
      if (curr_percentages.idle != prev_percentages.idle ||
          curr_percentages.user != prev_percentages.user || 
          curr_percentages.kernel != prev_percentages.kernel) {
        time_percentage_t* percentages = (time_percentage_t*)(message+1);
        percentages->idle = curr_percentages.idle;
        percentages->kernel = curr_percentages.kernel;
        percentages->user = curr_percentages.user;

        Send(display_ctrl, message, messagelen, &reply, 1);
        prev_percentages = curr_percentages;
      }
    }
    SaveThePlanet();
  }
}

void initialize() {
  tid_t display_controller = WhoIs(DISPLAY_CONTROLLER_NAME);
  tid_t train_controller = WhoIs(trains::TRAIN_CONTROLLER_NAME);
  tid_t switch_task = WhoIs(trains::SWITCH_TASK_NAME);

  char message = static_cast<char>(trains::special_cmd::GO);
  char reply;
  int replylen = Send(train_controller, &message, 1, &reply, 1);
  if (replylen != 1 || reply != static_cast<char>(trains::tc_reply::OK)) {
    DEBUG_LITERAL("Could not send GO command\r\n");
    return;
  }

  message = static_cast<char>(trains::special_cmd::RESET_MODE);
  replylen = Send(train_controller, &message, 1, &reply, 1);
  if (replylen != 1 || reply != static_cast<char>(trains::tc_reply::OK)) {
    DEBUG_LITERAL("Could not send RESET MODE command\r\n");
    return;
  }

  char speed_cmd_msg[1 + sizeof(trains::speed_cmd)];
  speed_cmd_msg[0] = static_cast<char>(trains::tc_msg_header::SPEED);
  trains::speed_cmd* scmd = (trains::speed_cmd*)(speed_cmd_msg+1);
  scmd->speed = 16;

  for (int train = 1; train <= 80; ++train) {
    scmd->train = train;
    Send(train_controller, speed_cmd_msg, 1 + sizeof(trains::speed_cmd), &reply, 1);
    if (replylen != 1 || reply != static_cast<char>(trains::tc_reply::OK)) {
      DEBUG_LITERAL("Could not send SPEED command\r\n");
    }
  }

  char switch_cmd_msg[1 + sizeof(trains::switch_cmd)];
  switch_cmd_msg[0] = static_cast<char>(display_msg_header::SWITCHES);
  trains::switch_cmd* swcmd = (trains::switch_cmd*)(switch_cmd_msg+1);

  for (size_t i = 0; i < num_solenoids; ++i) {
    swcmd->switch_num = active_solenoids[i];
    swcmd->switch_dir = trains::switch_dir_t::C;
    replylen = Send(switch_task, (char*)swcmd, sizeof(trains::switch_cmd), &reply, 1);
    if (replylen == 1 && reply == static_cast<char>(trains::tc_reply::OK)) {
      Send(display_controller, switch_cmd_msg, 1 + sizeof(trains::switch_cmd), &reply, 1);
    } else {
      DEBUG_LITERAL("Could not send SWITCH command\r\n");
    }
  }
}

void init_tasks() {
  Create(priority_t::PRIORITY_L1, display_controller_task);
  Create(priority_t::PRIORITY_L1, command_controller_task);
  Create(priority_t::PRIORITY_L2, timer_task);
  Create(priority_t::PRIORITY_IDLE, idle_task);

  Create(priority_t::PRIORITY_L1, initialize);
}

}
