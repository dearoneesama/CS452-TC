#include "gtkterm.hpp"
#include "kstddefs.hpp"
#include "user_syscall.h"
#include "rpi.hpp"
#include <etl/queue.h>

namespace gtkterm {

void gtkterm_rxnotifier() {
  tid_t server = MyParentTid();
  char reply;
  char msg = 'n';
  while (1) {
    Send(server, &msg, 1, &reply, 1);
    AwaitEvent(events_t::UART_R0);
  }
}

void gtkterm_txnotifier() {
  tid_t server = MyParentTid();
  char reply;
  char msg = 'n';
  while (1) {
    Send(server, &msg, 1, &reply, 1);
    AwaitEvent(events_t::UART_T0);
  }
}

void gtkterm_rxserver() {
  RegisterAs(GTK_RX_SERVER_NAME);
  tid_t notifier = Create(priority_t::PRIORITY_L1, gtkterm_rxnotifier);
  tid_t request_tid;
  char message[1];
  etl::queue<tid_t, 50> requester_queue;
  etl::queue<char, 512> char_queue;
  const char* reply = "o";
  bool notifier_is_parked = false;

  while (1) {
    int request = Receive((int*)&request_tid, message, 1);
    if (request <= 0) continue;

    switch (message[0]) {
      case 'n': { // notifier
        int rxlvl = UartReadRegister(0, rpi::UART_RXLVL);
        while (rxlvl != 0) {
          char_queue.push(UartReadRegister(0, rpi::UART_RHR));
          --rxlvl;
        }
        while (!requester_queue.empty() && !char_queue.empty()) {
          Reply(requester_queue.front(), &(char_queue.front()), 1);
          requester_queue.pop();
          char_queue.pop();
        }
        if (requester_queue.empty()) {
          notifier_is_parked = true;
        } else {
          Reply(notifier, reply, 1);
        }
        break;
      }
      case 'g': { // getc
        requester_queue.push(request_tid);
        while (!requester_queue.empty() && !char_queue.empty()) {
          Reply(requester_queue.front(), &(char_queue.front()), 1);
          requester_queue.pop();
          char_queue.pop();
        }
        if (!requester_queue.empty() && notifier_is_parked) {
          notifier_is_parked = false;
          Reply(notifier, reply, 1);
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
  etl::queue<char, 512> char_queue;
  const char* reply = "o";
  char c;
  bool notifier_is_parked = false;

  while (1) {
    int request = Receive((int*)&request_tid, message, 2);
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
          Reply(notifier, reply, 1);
        } else {
          notifier_is_parked = true;
        }
        break;
      }
      case 'p': { // putc
        char_queue.push(message[1]);
        Reply(request_tid, reply, 1);
        int txlvl = UartReadRegister(0, rpi::UART_TXLVL);
        while (!char_queue.empty() && txlvl != 0) {
          c = char_queue.front();
          char_queue.pop();
          UartWriteRegister(0, rpi::UART_THR, c);
          --txlvl;
        }
        if (!char_queue.empty() && notifier_is_parked) {
          notifier_is_parked = false;
          Reply(notifier, reply, 1);
        }
        break;
      }
      default: break;
    }
  }
}

void init_tasks() {
  Create(priority_t::PRIORITY_L1, gtkterm_txserver);
  Create(priority_t::PRIORITY_L1, gtkterm_rxserver);
}

}
