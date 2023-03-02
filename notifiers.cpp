#include "notifiers.hpp"
#include "rpi.hpp"
#include "user_syscall.h"
#include "servers.hpp"

void clocknotifier() {
  tid_t clock_server_tid = WhoIs("clock_server");

  if (!(STARTING_TASK_TID <= clock_server_tid && clock_server_tid <= ENDING_TASK_TID)) {
    DEBUG_LITERAL("Error querying the clock server tid from notifier...\r\n");
    return;
  }

  char reply;
  int value;
  char notify_message = static_cast<char>(CLOCK_MESSAGE::NOTIFY);
  while (1) {
    value = AwaitEvent(static_cast<char>(events_t::TIMER));
    if (value == 1) {
      value = Send(clock_server_tid, &notify_message, 1, &reply, 1);
      if (value == 1 && reply == static_cast<char>(CLOCK_REPLY::NOTIFY_OK)) {
        continue;
      } else {
        // something went wrong if we got here...
        DEBUG_LITERAL("Error sending notification to clock server...\r\n");
      }
    } else {
      DEBUG_LITERAL("Got unexpected value from AwaitEvent: ");
      uart_putui64(0, 0, value);
      DEBUG_LITERAL("\r\n");
    }
  }
}

// void tx_ctsnotifier() {
//   tid_t tx_notifier_tid = MyParentTid();
//   char cts_event = static_cast<char>(events_t::CTS_1);

//   int i;
//   char msg = 'o';
//   int value;
//   bool has_gone_down = false;
//   while (1) {
//     // DEBUG_LITERAL("cts waiting\r\n");
//     value = AwaitEvent(cts_event);
//     DEBUG_LITERAL("cts woke\r\n");
//     if (!is_clear_to_send(0, 1) && !has_gone_down) {
//       DEBUG_LITERAL("cts gone down\r\n");
//       has_gone_down = true;
//     } else if (is_clear_to_send(0, 1) && has_gone_down) {
//       DEBUG_LITERAL("cts back up\r\n");
//       Send(tx_notifier_tid, &msg, 1, &msg, 1);
//       has_gone_down = false;
//     }
//   }
// }

// void new_tx_uartnotifier() {
//   tid_t server_tid = MyParentTid();
//   char config[2];
//   Send(server_tid, "", 0, config, 2);
//   bool cts = config[0];
//   char uart_channel = config[1];

//   char message[2] = { 'n', 0 };
//   char reply = 0;
//   int value;
//   char event = uart_channel == 0 ? static_cast<char>(events_t::UART_T0) : static_cast<char>(events_t::UART_T1);

//   while (1) {
//     value = AwaitEvent(event);
//     message[1] = value;
//     Send(server_tid, message, 2, &reply, 1);
//   }
// }

// void tx_uartnotifier() {
//   tid_t server_tid = MyParentTid();
//   char config[2];
//   Send(server_tid, "", 0, config, 2);
//   bool cts = config[0];
//   char uart_channel = config[1];

//   char notify_message = 'n';
//   char reply = 0;
//   int value;
//   char event = uart_channel == 0 ? static_cast<char>(events_t::UART_T0) : static_cast<char>(events_t::UART_T1);
//   tid_t ctsnotifier;
//   if (uart_channel == 1) {
//     ctsnotifier = Create(PRIORITY_L1, tx_ctsnotifier);
//   }
//   // char cts_event = static_cast<char>(events_t::CTS_1);

//   while (1) {
//     if (is_uart_writable(0, uart_channel)) {
//       if (uart_channel == 1) {
//         DEBUG_LITERAL("tx notifier ");
//         uart_putc(0, 0, is_clear_to_send(0, uart_channel) + '0');
//         DEBUG_LITERAL("\r\n");
//       }
//       if (!cts || is_clear_to_send(0, uart_channel)) {
//         if (uart_channel == 1) {
//           DEBUG_LITERAL("tx notifier sending\r\n");
//         }
//         Send(server_tid, &notify_message, 1, &reply, 1);
//       }
//     }

//     if (uart_channel == 1) {
//       DEBUG_LITERAL("Going to sleep\r\n");
//     }
//     value = AwaitEvent(event);
//     if (uart_channel == 1) {
//       DEBUG_LITERAL("Woken up by interrupt\r\n");
//     }
//     if (value != 1) {
//       DEBUG_LITERAL("Error in tx notifier\r\n");
//     }

//     // cts case: we have received the tx interrupt but we still need to wait for the cts interrupt
//     // maybe have a separate notifier for this?
//     if (cts) {
//       // char msg;
//       // Receive((int*)&ctsnotifier, &msg, 1);
//       // Reply(ctsnotifier, &reply, 1);
//       // bool cts_has_gone_down = false;
//       // while (1) {
//       //   value = AwaitEvent(cts_event);
//       //   DEBUG_LITERAL("tx notifier got cts\r\n");
//       //   if (is_clear_to_send(0, uart_channel) && cts_has_gone_down) {
//       //     DEBUG_LITERAL("tx notifier clear to send\r\n");
//       //     break;
//       //   } else {
//       //     DEBUG_LITERAL("tx notifier cts gone down\r\n");
//       //     cts_has_gone_down = true;
//       //   }
//       // }
//     }
//   }
// }


// void rx_uartnotifier() {
//   tid_t server_tid = MyParentTid();
//   char uart_channel;
//   Send(server_tid, "", 0, &uart_channel, 1);
//   char notify_message = 'n';
//   char reply = 0;
//   int value;
//   char event = uart_channel == 0 ? static_cast<char>(events_t::UART_R0) : static_cast<char>(events_t::UART_R1);

//   while (1) {
//     // when we return from the syscall, interrupt should be disabled, and uart should be readable
//     // if (is_uart_readable(0, uart_channel)) {
//     //   Send(server_tid, &notify_message, 1, &reply, 1);
//     // }
//     Send(server_tid, &notify_message, 1, &reply, 1);
//     // this should enable interrupt
//     value = AwaitEvent(event);
//     if (value != 1) {
//       DEBUG_LITERAL("Error in rx notifier\r\n");
//     }
//   }
// }
