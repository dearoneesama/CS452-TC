#include "gtkterm.hpp"
#include "kstddefs.hpp"
#include "user_syscall_typed.hpp"
#include "rpi.hpp"
#include <etl/queue.h>

namespace gtkterm {

void gtkterm_rxnotifier() {
  tid_t server = MyParentTid();
  while (1) {
    SendValue(server, 'n', null_reply);
    AwaitEvent(events_t::UART_R0);
  }
}

void gtkterm_txnotifier() {
  tid_t server = MyParentTid();
  while (1) {
    SendValue(server, 'n', null_reply);
    AwaitEvent(events_t::UART_T0);
  }
}

static constexpr size_t MAX_QUEUED_CHARS = 1024;

void gtkterm_rxserver() {
  RegisterAs(GTK_RX_SERVER_NAME);
  tid_t notifier = Create(priority_t::PRIORITY_L1, gtkterm_rxnotifier);
  tid_t request_tid;
  char message[1];
  etl::queue<tid_t, 50> requester_queue;
  etl::queue<char, MAX_QUEUED_CHARS> char_queue;
  char reply = 'o';
  bool notifier_is_parked = false;

  while (1) {
    int request = ReceiveValue(request_tid, message);
    if (request <= 0) continue;

    switch (message[0]) {
      case 'n': { // notifier
        int rxlvl = UartReadRegister(0, rpi::UART_RXLVL);
        while (rxlvl != 0) {
          char_queue.push(UartReadRegister(0, rpi::UART_RHR));
          --rxlvl;
        }
        while (!requester_queue.empty() && !char_queue.empty()) {
          ReplyValue(requester_queue.front(), char_queue.front());
          requester_queue.pop();
          char_queue.pop();
        }
        if (requester_queue.empty()) {
          notifier_is_parked = true;
        } else {
          ReplyValue(notifier, reply);
        }
        break;
      }
      case 'g': { // getc
        requester_queue.push(request_tid);
        while (!requester_queue.empty() && !char_queue.empty()) {
          ReplyValue(requester_queue.front(), char_queue.front());
          requester_queue.pop();
          char_queue.pop();
        }
        if (!requester_queue.empty() && notifier_is_parked) {
          notifier_is_parked = false;
          ReplyValue(notifier, reply);
        }
      }
      default: break;
    }
  }
}

void gtkterm_txserver() {
  RegisterAs(GTK_TX_SERVER_NAME);
  tid_t notifier = Create(priority_t::PRIORITY_L1, gtkterm_txnotifier);
  tid_t request_tid;
  char message[2];
  etl::queue<char, MAX_QUEUED_CHARS> char_queue;
  char reply = 'o';
  char c;
  bool notifier_is_parked = false;

  while (1) {
    int request = ReceiveValue(request_tid, message);
    if (request <= 0) continue;

    switch (message[0]) {
      case 'n': { // notifier
        if (char_queue.empty()) {
          notifier_is_parked = true;
          break;
        }
        int txlvl = UartReadRegister(0, rpi::UART_TXLVL);
        while (!char_queue.empty() && txlvl != 0) {
          c = char_queue.front();
          char_queue.pop();
          UartWriteRegister(0, rpi::UART_THR, c);
          --txlvl;
        }
        if (!char_queue.empty()) {
          notifier_is_parked = false;
          ReplyValue(notifier, reply);
        } else {
          notifier_is_parked = true;
        }
        break;
      }
      case 'p': { // putc
        char_queue.push(message[1]);
        ReplyValue(request_tid, reply);
        int txlvl = UartReadRegister(0, rpi::UART_TXLVL);
        while (!char_queue.empty() && txlvl != 0) {
          c = char_queue.front();
          char_queue.pop();
          UartWriteRegister(0, rpi::UART_THR, c);
          --txlvl;
        }
        if (!char_queue.empty() && notifier_is_parked) {
          notifier_is_parked = false;
          ReplyValue(notifier, reply);
        }
        break;
      }
      default: break;
    }
  }
}

void init_tasks() {
  Create(priority_t::PRIORITY_L4, gtkterm_txserver);
  Create(priority_t::PRIORITY_L4, gtkterm_rxserver);
}

}
