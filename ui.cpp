#include "ui.hpp"
#include "gtkterm.hpp"
#include "user_syscall_typed.hpp"
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
  using namespace troll;

  RegisterAs(DISPLAY_CONTROLLER_NAME);
  auto gtkterm_tx = TaskFinder(gtkterm::GTK_TX_SERVER_NAME);

  tid_t request_tid;

  constexpr int user_row = 30;
  OutputControl<150, user_row> takeover;

  // headline
  using title_style = static_ansi_style_options<
    ansi_font::bold | ansi_font::italic, ansi_color::red, ansi_color::yellow
  >;
  auto title_str = sformat<50 + title_style::wrapper_str_size>(
    "{}{}{}",
    title_style::enabler_str(),
    pad<50>("painful train program", padding::middle),
    title_style::disabler_str()
  );
  takeover.enqueue(0, 0, title_str.data());

  // switch table
  constexpr size_t num_switches = 22;
  int switch_table_titles[num_switches] = {
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
    11, 12, 13, 14, 15, 16, 17, 18,
    153, 154, 155, 156
  };
  char switch_table_values[num_switches] = {
    '?', '?', '?', '?', '?', '?', '?', '?', '?', '?',
    '?', '?', '?', '?', '?', '?', '?', '?',
    '?', '?', '?', '?', 
  };
  auto switch_tab = make_tabulate<num_switches / 2, 9, 4>(
    static_ansi_style_options_none,
    tabulate_title_row_args{
      "Switch", switch_table_titles, switch_table_titles + num_switches,
      static_ansi_style_options<ansi_font::bold>{}, static_ansi_style_options_none
    },
    tabulate_elem_row_args{
      "Dir", switch_table_values,
      static_ansi_style_options<ansi_font::bold>{}, static_ansi_style_options_none
    }
  );
  size_t row = 4;
  for (auto sv : switch_tab) {
    takeover.enqueue(row++, 0, sv.data());
  }
  size_t switch_end = row;

  // notice
  takeover.enqueue(user_row - 1, 0, "| ");
  // user's input
  size_t user_input_char_count = 0;
  takeover.enqueue(user_row, 0, "> ");

  char message[128];
  const char reply = 'o';

  while (1) {
    while (!takeover.empty()) {
      auto sv = takeover.dequeue();
      Puts(gtkterm_tx(), 0, sv.data(), sv.size());
    }

    int request = ReceiveValue(request_tid, message);

    switch (static_cast<display_msg_header>(message[0])) {
      case display_msg_header::TIMER_CLOCK_MSG: { // time
        timer_clock_t* time = (timer_clock_t*)(message + 1);
        auto str = sformat<50>(
          "Uptime: {}:{}:{}:{}",
          time->hours,
          time->minutes,
          time->seconds,
          time->hundred_ms
        );
        takeover.enqueue(1, 0, str.data());
        ReplyValue(request_tid, reply);
        break;
      }
      case display_msg_header::IDLE_MSG : { // idle
        time_percentage_t* percentages = (time_percentage_t*) (message+1);
        auto str = pad<50>(sformat<50>(
          "Kernel: {}%  User: {}%  Idle: {}%",
          percentages->kernel,
          percentages->user,
          percentages->idle
        ), padding::left);
        takeover.enqueue(2, 0, str.data());
        ReplyValue(request_tid, reply);
        break;
      }
      case display_msg_header::SWITCHES: { // we assume that only changing active switches will go through here
        trains::switch_cmd* cmd = (trains::switch_cmd*) (message+1);
        char dir = cmd->switch_dir == trains::switch_dir_t::C ? 'C' : 'S';
        int switch_num = cmd->switch_num;
        int offset = switch_num <= 18 ? 1 : 135;
        switch_table_values[switch_num - offset] = dir;
        // redraw table
        size_t row = 4;
        for (auto sv : switch_tab) {
          takeover.enqueue(row++, 0, sv.data());
        }
        ReplyValue(request_tid, reply);
        break;
      }
      case display_msg_header::SENSOR_MSG: { // sensors
        message[request] = '\0';
        using style = static_ansi_style_options<ansi_font::bold>;
        auto str = sformat<sizeof message + style::wrapper_str_size + 10>(
          "{}Sensors:{} {}",
          style::enabler_str(),
          style::disabler_str(),
          message + 1
        );
        takeover.enqueue(switch_end + 1, 0, str.data());
        ReplyValue(request_tid, reply);
        break;
      }
      case display_msg_header::USER_INPUT: { // user input
        message[2] = '\0';
        takeover.enqueue(user_row, 2 + user_input_char_count, message + 1);
        ++user_input_char_count;
        ReplyValue(request_tid, reply);
        break;
      }
      case display_msg_header::USER_BACKSPACE: { // user pressed backspace
        --user_input_char_count;
        takeover.enqueue(user_row, 2 + user_input_char_count, " ");
        ReplyValue(request_tid, reply);
        break;
      }
      case display_msg_header::USER_ENTER: { // user pressed enter
        user_input_char_count = 0;
        takeover.enqueue(user_row, 2, nullptr);
        ReplyValue(request_tid, reply);
        break;
      }
      case display_msg_header::USER_NOTICE: { // string
        takeover.enqueue(user_row - 1, 2, nullptr);
        message[request] = '\0';
        takeover.enqueue(user_row - 1, 2, message + 1);
        ReplyValue(request_tid, reply);
        break;
      }
      default: {
        break;
      }
    }
  }
}

void command_controller_task() {
  auto gtkterm_rx = TaskFinder(gtkterm::GTK_RX_SERVER_NAME);
  auto display_controller = TaskFinder(DISPLAY_CONTROLLER_NAME);
  auto train_controller = TaskFinder(trains::TRAIN_CONTROLLER_NAME);
  auto reverse_task = TaskFinder(trains::REVERSE_TASK_NAME);
  auto switch_task = TaskFinder(trains::SWITCH_TASK_NAME);

  char command_buffer[1 + 64];
  command_buffer[0] = static_cast<char>(display_msg_header::USER_NOTICE);

  size_t curr_size = 1;
  trains::tc_reply reply;

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
    char c = Getc(gtkterm_rx(), 0);
    if (c == '\r') {
      SendValue(display_controller(), display_msg_header::USER_ENTER, null_reply);
      SendValue(display_controller(), command_buffer, curr_size, null_reply);

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
          int replylen = SendValue(train_controller(), speed_cmd_msg, reply);
          if (replylen == 1 && reply == trains::tc_reply::OK) {
            valid = true;
          }
        }
      } else if (troll::sscan(command_buffer + 1, curr_size - 1, "rv {}", arg1)) {
        if (is_valid_train(arg1)) {
          rcmd.train = arg1;
          int replylen = SendValue(reverse_task(), rcmd, reply);
          if (replylen == 1 && reply == trains::tc_reply::OK) {
            valid = true;
          }
        }
      } else if (troll::sscan(command_buffer + 1, curr_size - 1, "sw {} {}", arg1, char_arg)) {
        if (is_valid_solenoid(arg1) && is_valid_solenoid_dir(char_arg)) {
          // filter out the solenoids that are not even on the track
          for (size_t i = 0; i < num_solenoids; ++i) {
            if (arg1 == active_solenoids[i]) {
              swcmd->switch_num = arg1;
              swcmd->switch_dir = char_arg == 'S' ? trains::switch_dir_t::S : trains::switch_dir_t::C;
              int replylen = SendValue(switch_task(), *swcmd, reply);
              if (replylen == 1 && reply == trains::tc_reply::OK) {
                SendValue(display_controller(), switch_cmd_msg, reply);
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
        const char invalid_msg[] = " is invalid!";
        troll::snformat(command_buffer + curr_size, sizeof command_buffer - curr_size, invalid_msg);
        SendValue(display_controller(), command_buffer, curr_size + LEN_LITERAL(invalid_msg), null_reply);
      }
      curr_size = 1;

    } else if (c == 8 && curr_size > 1) { // back space
      curr_size--;
      SendValue(display_controller(), display_msg_header::USER_BACKSPACE, null_reply);

    } else if (curr_size < 65) {
      command_buffer[curr_size++] = c;
      char input_msg[2];
      input_msg[0] = static_cast<char>(display_msg_header::USER_INPUT);
      input_msg[1] = c;
      SendValue(display_controller(), input_msg, null_reply);
    }
  }
}

void timer_task() {
  auto display_controller = TaskFinder(DISPLAY_CONTROLLER_NAME);
  auto clock_server = TaskFinder("clock_server");

  char message[1 + sizeof(timer_clock_t)];
  message[0] = display_msg_header::TIMER_CLOCK_MSG;
  timer_clock_t *time = (timer_clock_t*)(message+1);
  time->hours = 0;
  time->minutes = 0;
  time->seconds = 0;
  time->hundred_ms = 0;

  uint32_t current_time = Time(clock_server());

  while (1) {
    current_time = DelayUntil(clock_server(), current_time + 10); // every 100ms
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
    SendValue(display_controller(), message, null_reply);
  }
}

// todo: move this somewhere else
void idle_task() {
  auto display_ctrl = TaskFinder(DISPLAY_CONTROLLER_NAME);

  time_percentage_t prev_percentages = {0, 0, 0};
  time_percentage_t curr_percentages = {0, 0, 0};
  time_distribution_t td = {0, 0, 0};
  constexpr size_t messagelen = 1 + sizeof(time_percentage_t);
  char message[messagelen];
  message[0] = static_cast<char>(display_msg_header::IDLE_MSG);
  size_t entries = 10;

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

        SendValue(display_ctrl(), message, null_reply);
        prev_percentages = curr_percentages;
      }
    }
    SaveThePlanet();
  }
}

void initialize() {
  auto display_controller = TaskFinder(DISPLAY_CONTROLLER_NAME);
  auto train_controller = TaskFinder(trains::TRAIN_CONTROLLER_NAME);
  auto switch_task = TaskFinder(trains::SWITCH_TASK_NAME);

  trains::tc_reply reply;
  int replylen = SendValue(train_controller(), trains::tc_msg_header::GO_CMD, reply);
  if (replylen != 1 || reply != trains::tc_reply::OK) {
    DEBUG_LITERAL("Could not send GO command\r\n");
    return;
  }

  replylen = SendValue(train_controller(), trains::tc_msg_header::SET_RESET_MODE, reply);
  if (replylen != 1 || reply != trains::tc_reply::OK) {
    DEBUG_LITERAL("Could not send RESET MODE command\r\n");
    return;
  }

  char speed_cmd_msg[1 + sizeof(trains::speed_cmd)];
  speed_cmd_msg[0] = static_cast<char>(trains::tc_msg_header::SPEED);
  trains::speed_cmd* scmd = (trains::speed_cmd*)(speed_cmd_msg+1);
  scmd->speed = 16;

  for (int train = 1; train <= 80; ++train) {
    scmd->train = train;
    SendValue(train_controller(), speed_cmd_msg, reply);
    if (replylen != 1 || reply != trains::tc_reply::OK) {
      DEBUG_LITERAL("Could not send SPEED command\r\n");
    }
  }

  char switch_cmd_msg[1 + sizeof(trains::switch_cmd)];
  switch_cmd_msg[0] = static_cast<char>(display_msg_header::SWITCHES);
  trains::switch_cmd* swcmd = (trains::switch_cmd*)(switch_cmd_msg+1);

  for (size_t i = 0; i < num_solenoids; ++i) {
    swcmd->switch_num = active_solenoids[i];
    swcmd->switch_dir = trains::switch_dir_t::C;
    replylen = SendValue(switch_task(), *swcmd, reply);
    if (replylen == 1 && reply == trains::tc_reply::OK) {
      SendValue(display_controller(), switch_cmd_msg, reply);
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
