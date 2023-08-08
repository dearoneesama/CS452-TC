#include <etl/circular_buffer.h>
#include <etl/fixed_iterator.h>
#include <troll_util/format_scan.hpp>
#include <troll_util/utils.hpp>
#include "ui.hpp"
#include "kern/gtkterm.hpp"
#include "kern/kstddefs.hpp"
#include "tcmd.hpp"
#include "kern/rpi.hpp"
#include "track_consts.hpp"

namespace ui {

ui_sender &out() {
  static ui_sender sender;
  return sender;
}

const char *manual[] = {
  "Manual",
  "tr <train_num> <speed_level>           Set train command",
  "rv <train_num>                         Set train to opposite direction",
  "sw <switch_num> S|C                    Set turnout direction",
  "init <train_num> <node_name> <offset>  Set train position for tracking",
  "deinit <train_num>                     Remove train from tracking",
  "goto <train_num> <node_name> <offset>  Make train go to a position",
  "st                                     Stop all trains",
  "q                                      Quit",
  "",
  "This program was compiled on " __DATE__ " " __TIME__ " for track "
#if IS_TRACK_A == 1
  "A "
#else
  "B "
#endif
#if NO_CTS
  "without "
#else
  "with "
#endif
  "CTS.",
};

template<size_t N, size_t I = 0, class Tab, class OC>
[[noreturn]] std::enable_if_t<I == N, void>
do_patch_lock(Tab &, OC&, etl::string_view, size_t, size_t, size_t, bool) {
  __builtin_unreachable();
}

template<size_t N, size_t I = 0, class Tab, class OC>
std::enable_if_t<I < N, void>
do_patch_lock(Tab &tab, OC &takeover, etl::string_view data, size_t idx, size_t row_offset, size_t col_offset, bool is_sensor) {
  if (idx != 0) {
    return do_patch_lock<N, I + 1>(tab, takeover, data, idx - 1, row_offset, col_offset, is_sensor);
  }
  auto [row, col, patch] = tab.template patch_str<I + 1>(int{!is_sensor}, data);
  takeover.enqueue(row + row_offset, col + col_offset, patch.data());
}

void display_controller_task() {
  using namespace troll;

  RegisterAs(DISPLAY_CONTROLLER_NAME);
  auto gtkterm_tx = TaskFinder(gtkterm::GTK_TX_SERVER_NAME);

  tid_t request_tid;

  // how many rows are reserved; 59th row (0-based) is the location for naive Putc
  constexpr int user_rows = 64;
  output_control<150, user_rows> takeover;

  // headline
  using title_style = static_ansi_style_options<
    ansi_font::bold | ansi_font::italic, ansi_color::red, ansi_color::yellow
  >;
  auto title_str = sformat<78 + title_style::wrapper_str_size>(
    "{}{}{}",
    title_style::enabler_str(),
    pad<78>("painful train program", padding::middle),
    title_style::disabler_str()
  );
  takeover.enqueue(0, 0, title_str.data());

  constexpr int col_offset = 1;

  // switch table
  char switch_table_val_dummy[1] = {'?'};
  auto switch_tab = make_tabulate<tracks::num_switches / 2, 9, 6>(
    static_ansi_style_options<ansi_font::dim>{},
    tabulate_title_row_args{
      "Switch", tracks::valid_switches().begin(), tracks::valid_switches().end(),
      static_ansi_style_options<ansi_font::bold>{}, static_ansi_style_options_none
    },
    tabulate_elem_row_args{
      "Dir", ::etl::fixed_iterator<char *>(switch_table_val_dummy),
      static_ansi_style_options<ansi_font::bold>{}, static_ansi_style_options_none
    }
  );
  constexpr size_t switch_table_row = 5;
  size_t row = switch_table_row;
  for (auto sv : switch_tab) {
    takeover.enqueue(row++, col_offset, sv.data());
  }

  // sensor reads
  ::etl::circular_buffer<sensor_read, 12> sensor_reads;
  auto sensor_read_title_it = troll::it_transform(
    sensor_reads.rbegin(), sensor_reads.rend(),
    [](sensor_read const &sr) { return static_cast<::etl::string_view>(sr.sensor); }
  );
  auto sensor_read_tick_it = troll::it_transform(
    sensor_reads.rbegin(), sensor_reads.rend(),
    [](sensor_read const &sr) { return sr.tick; }
  );
  auto sensor_tab = make_tabulate<6, 9, 11>(
    static_ansi_style_options<ansi_font::dim>{},
    tabulate_title_row_args{
      "Sensor", sensor_read_title_it.begin(), sensor_read_title_it.end(),
      static_ansi_style_options<ansi_font::bold>{}, static_ansi_style_options_none
    },
    tabulate_elem_row_args{
      "Tick", sensor_read_tick_it.begin(),
      static_ansi_style_options<ansi_font::bold>{}, static_ansi_style_options_none
    }
  );
  constexpr size_t sensor_table_row = 15;
  row = sensor_table_row;
  for (auto sv : sensor_tab) {
    takeover.enqueue(row++, col_offset, sv.data());
  }
  int last_sensor_redraw = 0;

  // train reads
  auto &valid_trains = tracks::valid_trains();
  using vt_type = std::remove_reference_t<decltype(valid_trains)>;
  //
  int train_read_int_dummy[1] = {0};
  ::etl::string<11> train_read_str_dummy[1] = {"?"};
  ::etl::fixed_iterator<int *> train_read_int_it_dummy {train_read_int_dummy};
  ::etl::fixed_iterator<::etl::string<11> *> train_read_str_it_dummy {train_read_str_dummy};

  auto train_tab = make_tabulate<vt_type::SIZE, 9, 11>(
    static_ansi_style_options<ansi_font::dim>{},
    tabulate_title_row_args{
      "Train", valid_trains.begin(), valid_trains.end(),
      static_ansi_style_options<ansi_font::bold>{}, static_ansi_style_options_none
    },
    tabulate_elem_row_args{
      "Cmd", train_read_int_it_dummy,
      static_ansi_style_options<ansi_font::bold>{}, static_ansi_style_options_none
    },
    tabulate_elem_row_args{
      "Dest.", train_read_str_it_dummy,
      static_ansi_style_options<ansi_font::bold>{}, static_ansi_style_options_none
    },
    tabulate_elem_row_args{
      "Speed", train_read_int_it_dummy,
      static_ansi_style_options<ansi_font::bold>{}, static_ansi_style_options_none
    },
    tabulate_elem_row_args{
      "Pos.", train_read_str_it_dummy,
      static_ansi_style_options<ansi_font::bold>{}, static_ansi_style_options_none
    },
    tabulate_elem_row_args{
      "dt", train_read_int_it_dummy,
      static_ansi_style_options<ansi_font::bold>{}, static_ansi_style_options_none
    },
    tabulate_elem_row_args{
      "dd", train_read_int_it_dummy,
      static_ansi_style_options<ansi_font::bold>{}, static_ansi_style_options_none
    }
  );
  constexpr size_t train_table_row = 25;
  row = train_table_row;
  for (auto sv : train_tab) {
    takeover.enqueue(row++, col_offset, sv.data());
  }

  auto get_train_patch_idx = [&valid_trains](int num) {
    auto it = std::find(valid_trains.begin(), valid_trains.end(), num);
    return it - valid_trains.begin();
  };

  // user's input
  constexpr size_t user_input_row = 51;
  using blink_style = static_ansi_style_options<ansi_font::blink>;
  auto blinking_underscore = sformat<1 + blink_style::wrapper_str_size>(
    "{}_{}", blink_style::enabler_str(), blink_style::disabler_str()
  );
  size_t user_input_char_count = 0;
  takeover.enqueue(user_input_row, 0, "> ");
  takeover.enqueue(user_input_row, 2, blinking_underscore.data());

  // notices
  constexpr size_t num_notices = 10;
  ::etl::circular_buffer<::etl::string<256>, num_notices> notices;
  for (size_t i = 0; i < num_notices; ++i) {
    takeover.enqueue(user_input_row - 1 - i, 0, "| ");
  }

  // manual
  constexpr size_t manual_row = 53;
  using manual_style = static_ansi_style_options<ansi_font::dim>;
  for (size_t i = 0; i < sizeof(manual) / sizeof(manual[0]); ++i) {
    auto str = sformat<100 + manual_style::wrapper_str_size>(
      "{}{}{}",
      manual_style::enabler_str(),
      manual[i],
      manual_style::disabler_str()
    );
    takeover.enqueue(manual_row + i, col_offset, str.data());
  }

  // reservations
  const char *train_lock_title[2] = { "Reserved sensors", "Locks" };
  const char *train_lock_elems_dummy[2] = { "", "" };
  auto train_lock_tab = make_tabulate<2, 9, 20>(
    static_ansi_style_options<ansi_font::dim>{},
    tabulate_title_row_args{
      "Train", train_lock_title, train_lock_title + 2,
      static_ansi_style_options<ansi_font::bold>{},
    },
#define TABULATE_ELEM_ROW_ARGS(I) \
    tabulate_elem_row_args{ \
      valid_trains.at(I), train_lock_elems_dummy, \
      static_ansi_style_options_none \
    }
    TABULATE_ELEM_ROW_ARGS(0),
    TABULATE_ELEM_ROW_ARGS(1),
    TABULATE_ELEM_ROW_ARGS(2),
    TABULATE_ELEM_ROW_ARGS(3),
    TABULATE_ELEM_ROW_ARGS(4),
    TABULATE_ELEM_ROW_ARGS(5)
#undef TABULATE_ELEM_ROW_ARGS
  );
  constexpr size_t train_lock_row = 25;
  constexpr size_t train_lock_col = 80;
  row = train_lock_row;
  for (auto sv : train_lock_tab) {
    takeover.enqueue(row++, train_lock_col, sv.data());
  }

  utils::enumed_class<display_msg_header, char[256]> message;
  const char reply = 'o';

  while (1) {
    while (!takeover.empty()) {
      auto sv = takeover.dequeue();
      Puts(gtkterm_tx(), 0, sv.data(), sv.size());
    }

    if (ReceiveValue(request_tid, message) < 1) {
      continue;
    }

    switch (message.header) {
      case display_msg_header::TIMER_CLOCK_MSG: { // time
        auto &time = message.data_as<timer_clock_t>();
        auto str = pad<50>(sformat<50>(
          "Uptime: {}:{}:{}:{}",
          time.hours,
          time.minutes,
          time.seconds,
          time.hundred_ms
        ), padding::left);
        takeover.enqueue(2, col_offset, str.data());
        ReplyValue(request_tid, reply);
        break;
      }
      case display_msg_header::IDLE_MSG : { // idle
        auto &percentages = message.data_as<time_percentage_t>();
        auto str = pad<50>(sformat<50>(
          "Kernel: {}%  User: {}%  Idle: {}%",
          percentages.kernel,
          percentages.user,
          percentages.idle
        ), padding::left);
        takeover.enqueue(3, col_offset, str.data());
        ReplyValue(request_tid, reply);
        break;
      }
      case display_msg_header::SWITCHES: { // we assume that only changing active switches will go through here
        auto &cmd = message.data_as<ui::switch_read>();
        char dir = cmd.switch_dir == tcmd::switch_dir_t::C ? 'C' : 'S';
        int switch_num = cmd.switch_num;
        int offset = switch_num <= 18 ? 1 : 135;
        // patch table
        auto [row, col, patch] = switch_tab.patch_str<1>(switch_num - offset, dir);
        takeover.enqueue(row + switch_table_row, col + col_offset, patch.data());
        ReplyValue(request_tid, reply);
        break;
      }
      case display_msg_header::SENSOR_MSG: { // sensors
        auto &data = message.data_as<sensor_read>();
        // do some special handling to avoid same sensor spamming screen
        auto num_reports = sensor_reads.size();
        auto &last = sensor_reads.back();
        auto last_sensor = static_cast<::etl::string_view>(last.sensor);
        auto data_sensor = static_cast<::etl::string_view>(data.sensor);
        if (num_reports > 1 && last_sensor == data_sensor && sensor_reads[num_reports - 2].sensor == data_sensor) {
          // last.tick = data.tick; not necessary
          auto [row, col, patch] = sensor_tab.patch_str<1>(0, data.tick);
          takeover.enqueue(row + sensor_table_row, col + col_offset, patch.data());
        } else {
          sensor_reads.push(data);
          if (data.tick - last_sensor_redraw > 25) {
            // redraw table
            sensor_read_title_it.reset_src_iterator(sensor_reads.rbegin(), sensor_reads.rend());
            sensor_read_tick_it.reset_src_iterator(sensor_reads.rbegin(), sensor_reads.rend());
            sensor_tab.reset_src_iterator(sensor_read_title_it.begin(), sensor_read_title_it.end(), sensor_read_tick_it.begin());
            row = sensor_table_row;
            for (auto sv : sensor_tab) {
              takeover.enqueue(row++, col_offset, sv.data());
            }
            last_sensor_redraw = data.tick;
          }
        }
        ReplyValue(request_tid, reply);
        break;
      }
      case display_msg_header::TRAIN_READ: { // train status update
        ReplyValue(request_tid, null_reply);
        auto &tr = message.data_as<train_read>();
        auto idx = get_train_patch_idx(tr.num);
        // patch the table
        auto [row1, col1, patch1] = train_tab.patch_str<1>(idx, tr.cmd);
        takeover.enqueue(row1 + train_table_row, col1 + col_offset, patch1.data());
        auto [row2, col2, patch2] = train_tab.patch_str<2>(idx, stringify_pos(tr.dest));
        takeover.enqueue(row2 + train_table_row, col2 + col_offset, patch2.data());
        auto [row3, col3, patch3] = train_tab.patch_str<3>(idx, tr.speed);
        takeover.enqueue(row3 + train_table_row, col3 + col_offset, patch3.data());
        auto [row4, col4, patch4] = train_tab.patch_str<4>(idx, stringify_pos(tr.pos));
        takeover.enqueue(row4 + train_table_row, col4 + col_offset, patch4.data());
        auto [row5, col5, patch5] = train_tab.patch_str<5>(idx, tr.delta_t);
        takeover.enqueue(row5 + train_table_row, col5 + col_offset, patch5.data());
        auto [row6, col6, patch6] = train_tab.patch_str<6>(idx, tr.delta_d);
        takeover.enqueue(row6 + train_table_row, col6 + col_offset, patch6.data());
        break;
      }
      case display_msg_header::SENSOR_LOCK:
      case display_msg_header::SWITCH_LOCK: {
        ReplyValue(request_tid, null_reply);
        auto &read = message.data_as<sensor_lock>();
        auto idx = get_train_patch_idx(read.train);
        do_patch_lock<vt_type::SIZE>(train_lock_tab, takeover, read.str, idx, train_lock_row, train_lock_col, message.header == display_msg_header::SENSOR_LOCK);
        break;
      }
      case display_msg_header::USER_INPUT: { // user input
        takeover.enqueue(user_input_row, 2 + user_input_char_count, message.data);
        takeover.enqueue(user_input_row, 2 + user_input_char_count + 1, blinking_underscore.data());
        ++user_input_char_count;
        ReplyValue(request_tid, reply);
        break;
      }
      case display_msg_header::USER_BACKSPACE: { // user pressed backspace
        --user_input_char_count;
        takeover.enqueue(user_input_row, 2 + user_input_char_count, "  ");
        takeover.enqueue(user_input_row, 2 + user_input_char_count, blinking_underscore.data());
        ReplyValue(request_tid, reply);
        break;
      }
      case display_msg_header::USER_ENTER: { // user pressed enter
        user_input_char_count = 0;
        takeover.enqueue(user_input_row, 2, nullptr);
        takeover.enqueue(user_input_row, 2, blinking_underscore.data());
        ReplyValue(request_tid, reply);
        break;
      }
      case display_msg_header::USER_NOTICE: { // string
        ReplyValue(request_tid, reply);
        notices.push(message.data);
        // redraw rows
        row = user_input_row - 1;
        auto rend = notices.rend();
        for (auto nt = notices.rbegin(); nt != rend; ++nt, --row) {
          takeover.enqueue(row, 2, nullptr);
          takeover.enqueue(row, 2, nt->data());
        }
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
  auto train_task = TaskFinder(tcmd::TRAIN_TASK_NAME);
  auto reverse_task = TaskFinder(tcmd::REVERSE_TASK_NAME);
  auto switch_task = TaskFinder(tcmd::SWITCH_TASK_NAME);
  auto traffic_task = TaskFinder(traffic::TRAFFIC_SERVER_TASK_NAME);

  utils::enumed_class<display_msg_header, char[64]> command_buffer;
  command_buffer.header = display_msg_header::USER_NOTICE;

  size_t curr_size = 0;
  tcmd::tc_reply reply;

  auto is_valid_train = [](int arg) {
    auto &vt = tracks::valid_trains();
    return std::find(vt.begin(), vt.end(), arg) != vt.end();
  };
  auto is_valid_speed = [](int arg) {
    return (0 <= arg && arg <= 14) || (16 <= arg && arg <= 30);
  };
  auto is_valid_solenoid = [](int arg) {
    auto &vs = tracks::valid_switches();
    return std::find(vs.begin(), vs.end(), arg) != vs.end();
  };
  auto is_valid_solenoid_dir = [](int arg) {
    return arg == 'S' || arg == 'C';
  };
  auto is_valid_node = [](const auto &s) {
    auto &vn = tracks::valid_nodes();
    return vn.find(s) != vn.end();
  };
  auto is_valid_offset = [](int arg) {
    return arg >= 0;
  };
  auto toupper_str = [](etl::istring &s) {
    for (auto &c : s) {
      if ('a' <= c && c <= 'z') {
        c = c - 'a' + 'A';
      }
    }
  };

  utils::enumed_class<tcmd::tc_msg_header, tcmd::speed_cmd> speed_cmd_msg;
  speed_cmd_msg.header = tcmd::tc_msg_header::SPEED;

  tcmd::reverse_cmd rcmd = {0};

  utils::enumed_class<display_msg_header, tcmd::switch_cmd> switch_cmd_msg;
  switch_cmd_msg.header = display_msg_header::SWITCHES;
  switch_cmd_msg.data.switch_dir = tcmd::switch_dir_t::NONE;
  switch_cmd_msg.data.switch_num = 0;

  while (1) {
    char c = Getc(gtkterm_rx(), 0);
    if (c == '\r') {
      out().send_value(display_msg_header::USER_ENTER);
      command_buffer.data[curr_size] = '\0';
      out().send_value(command_buffer);

      int arg1 = 0, arg2 = 0;
      char char_arg = 0;
      ::etl::string<5> str_arg;
      bool valid = false;

      if (troll::sscan(command_buffer.data, curr_size, "tr {} {}", arg1, arg2)) {
        if (is_valid_train(arg1) && is_valid_speed(arg2)) {
          speed_cmd_msg.data.train = arg1;
          speed_cmd_msg.data.speed = arg2;
          int replylen = SendValue(train_task(), speed_cmd_msg, reply);
          if (replylen == 1 && reply == tcmd::tc_reply::OK) {
            valid = true;
          }
        }
      } else if (troll::sscan(command_buffer.data, curr_size, "rv {}", arg1)) {
        if (is_valid_train(arg1)) {
          rcmd.train = arg1;
          int replylen = SendValue(reverse_task(), rcmd, reply);
          if (replylen == 1 && reply == tcmd::tc_reply::OK) {
            valid = true;
          }
        }
      } else if (troll::sscan(command_buffer.data, curr_size, "sw {} {}", arg1, char_arg)) {
        if (is_valid_solenoid(arg1) && is_valid_solenoid_dir(char_arg)) {
          switch_cmd_msg.data.switch_num = arg1;
          switch_cmd_msg.data.switch_dir = char_arg == 'S' ? tcmd::switch_dir_t::S : tcmd::switch_dir_t::C;
          int replylen = SendValue(switch_task(), switch_cmd_msg.data, reply);
          valid = replylen == 1 && reply == tcmd::tc_reply::OK;
        }
      } else if (troll::sscan(command_buffer.data, curr_size, "st")) {
        traffic::traffic_reply_msg reply {};
        SendValue(traffic_task(), traffic::traffic_msg_header::TRAINS_STOP, reply);
        valid = reply == traffic::traffic_reply_msg::OK;
      } else if (troll::sscan(command_buffer.data, curr_size, "init {} {} {}", arg1, str_arg, arg2)) {
        toupper_str(str_arg);
        if (is_valid_train(arg1) && is_valid_node(str_arg) && is_valid_offset(arg2)) {
          traffic::traffic_reply_msg reply {};
          SendValue(traffic_task(), utils::enumed_class {
            traffic::traffic_msg_header::TRAIN_POS_INIT,
            traffic::train_pos_init_msg {arg1, str_arg, arg2},
          }, reply);
          valid = reply == traffic::traffic_reply_msg::OK;
        }
      } else if (troll::sscan(command_buffer.data, curr_size, "deinit {}", arg1)) {
        if (is_valid_train(arg1)) {
          traffic::traffic_reply_msg reply {};
          SendValue(traffic_task(), utils::enumed_class {
            traffic::traffic_msg_header::TRAIN_POS_DEINIT,
            traffic::train_deinit_msg {arg1},
          }, reply);
          valid = reply == traffic::traffic_reply_msg::OK;
        }
      } else if (troll::sscan(command_buffer.data, curr_size, "goto {} {} {}", arg1, str_arg, arg2)) {
        toupper_str(str_arg);
        if (is_valid_train(arg1) && is_valid_node(str_arg) && is_valid_offset(arg2)) {
          traffic::traffic_reply_msg reply {};
          SendValue(traffic_task(), utils::enumed_class {
            traffic::traffic_msg_header::TRAIN_POS_GOTO,
            traffic::train_pos_goto_msg {arg1, str_arg, arg2},
          }, reply);
          valid = reply == traffic::traffic_reply_msg::OK;
        }
      } else if (curr_size == 1 && command_buffer.data[0] == 'q') {
        Terminate();
      }

      if (!valid) {
        out().send_notice("Command was not delivered or it was not valid.");
      }
      curr_size = 0;

    } else if (c == 8 && curr_size >= 1) { // back space
      curr_size--;
      out().send_value(display_msg_header::USER_BACKSPACE);

    } else if (curr_size < 65) {
      command_buffer.data[curr_size++] = c;
      utils::enumed_class<display_msg_header, char[2]> input_msg {
        display_msg_header::USER_INPUT,
        {c, '\0'}
      };
      out().send_value(input_msg);
    }
  }
}

void timer_task() {
  auto clock_server = TaskFinder("clock_server");

  utils::enumed_class<display_msg_header, timer_clock_t> message;
  message.header = display_msg_header::TIMER_CLOCK_MSG;
  auto &time = message.data;
  time.hours = 0;
  time.minutes = 0;
  time.seconds = 0;
  time.hundred_ms = 0;

  int current_time = Time(clock_server());

  while (1) {
    if (current_time < 1) {
      // because DelayUntil() may fail if we miss a tick
      current_time = Time(clock_server());
    } else {
      current_time = DelayUntil(clock_server(), current_time + 10); // every 100ms
    }
    time.hundred_ms++;
    if (time.hundred_ms == 10) {
      time.seconds++;
      time.hundred_ms = 0;
    }
    if (time.seconds == 60) {
      time.minutes++;
      time.seconds = 0;
    }
    if (time.minutes == 60) {
      time.hours++;
      time.minutes = 0;
    }
    out().send_value(message);
  }
}

// todo: move this somewhere else
void idle_task() {
  time_percentage_t prev_percentages = {0, 0, 0};
  time_percentage_t curr_percentages = {0, 0, 0};
  time_distribution_t td = {0, 0, 0};

  utils::enumed_class<display_msg_header, time_percentage_t> message;
  message.header = display_msg_header::IDLE_MSG;
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
        auto &percentages = message.data;
        percentages.idle = curr_percentages.idle;
        percentages.kernel = curr_percentages.kernel;
        percentages.user = curr_percentages.user;

        out().send_value(message);
        prev_percentages = curr_percentages;
      }
    }
    SaveThePlanet();
  }
}

void initialize() {
  auto train_task = TaskFinder(tcmd::TRAIN_TASK_NAME);
  auto switch_task = TaskFinder(tcmd::SWITCH_TASK_NAME);

  out().send_notice("Initializing...");

  tcmd::tc_reply reply;
  int replylen = SendValue(train_task(), tcmd::tc_msg_header::GO_CMD, reply);
  if (replylen != 1 || reply != tcmd::tc_reply::OK) {
    out().send_notice("Could not send GO command\r\n");
    return;
  }

  replylen = SendValue(train_task(), tcmd::tc_msg_header::SET_RESET_MODE, reply);
  if (replylen != 1 || reply != tcmd::tc_reply::OK) {
    out().send_notice("Could not send RESET MODE command\r\n");
    return;
  }

  utils::enumed_class<tcmd::tc_msg_header, tcmd::speed_cmd> speed_cmd_msg;
  speed_cmd_msg.header = tcmd::tc_msg_header::SPEED;
  speed_cmd_msg.data.speed = 16;

  for (auto train : tracks::valid_trains()) {
    speed_cmd_msg.data.train = train;
    SendValue(train_task(), speed_cmd_msg, reply);
    if (replylen != 1 || reply != tcmd::tc_reply::OK) {
      out().send_notice("Could not send SPEED command");
      return;
    }
  }

  utils::enumed_class<display_msg_header, tcmd::switch_cmd> switch_cmd_msg;
  switch_cmd_msg.header = display_msg_header::SWITCHES;

  for (auto sw : tracks::valid_switches()) {
    switch_cmd_msg.data.switch_num = sw;
    switch_cmd_msg.data.switch_dir = tcmd::switch_dir_t::S;
    replylen = SendValue(switch_task(), switch_cmd_msg.data, reply);
    if (replylen != 1 || reply != tcmd::tc_reply::OK) {
      out().send_notice("Could not send SWITCH command");
      return;
    }
  }

  out().send_notice("Initialization complete.");
}

void init_tasks() {
  Create(priority_t::PRIORITY_L4, display_controller_task);
  Create(priority_t::PRIORITY_L4, command_controller_task);
  Create(priority_t::PRIORITY_L5, timer_task);
  Create(priority_t::PRIORITY_IDLE, idle_task);

  Create(priority_t::PRIORITY_L5, initialize);
}

}
