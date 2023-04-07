#include "gtkterm.hpp"
#include "kstddefs.hpp"
#include "user_syscall_typed.hpp"
#include "rpi.hpp"
#include "servers.hpp"
#include "../generic/utils.hpp"
#include <etl/queue.h>

namespace gtkterm {

void gtkterm_rxnotifier() {
  tid_t server = MyParentTid();
  while (1) {
    SendValue(server, UART_MESSAGE::RX_NOTIFIER, null_reply);
    AwaitEvent(events_t::UART_R0);
  }
}

void gtkterm_txnotifier() {
  tid_t server = MyParentTid();
  while (1) {
    SendValue(server, UART_MESSAGE::TX_NOTIFIER, null_reply);
    AwaitEvent(events_t::UART_T0);
  }
}

static constexpr size_t MAX_QUEUED_CHARS = 1024*10;

void gtkterm_rxserver() {
  RegisterAs(GTK_RX_SERVER_NAME);
  tid_t notifier = Create(priority_t::PRIORITY_L1, gtkterm_rxnotifier);
  tid_t request_tid;
  UART_MESSAGE message;
  etl::queue<tid_t, 50> requester_queue;
  etl::queue<char, MAX_QUEUED_CHARS> char_queue;
  bool notifier_is_parked = false;

  while (1) {
    int request = ReceiveValue(request_tid, message);
    if (request <= 0) continue;

    switch (message) {
      case UART_MESSAGE::RX_NOTIFIER: { // notifier
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
          ReplyValue(notifier, UART_REPLY::OK);
        }
        break;
      }
      case UART_MESSAGE::GETC: { // getc
        requester_queue.push(request_tid);
        while (!requester_queue.empty() && !char_queue.empty()) {
          ReplyValue(requester_queue.front(), char_queue.front());
          requester_queue.pop();
          char_queue.pop();
        }
        if (!requester_queue.empty() && notifier_is_parked) {
          notifier_is_parked = false;
          ReplyValue(notifier, UART_REPLY::OK);
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
  utils::enumed_class<UART_MESSAGE, char[2048]> message;
  etl::queue<char, MAX_QUEUED_CHARS> char_queue;
  bool notifier_is_parked = false;

  auto try_write = [&char_queue] {
    int txlvl = UartReadRegister(0, rpi::UART_TXLVL);
    while (txlvl > 0 && !char_queue.empty()) {
      char buf[33];
      auto writable = std::min({
        txlvl,
        static_cast<int>(sizeof buf / sizeof buf[0] - 1),
        static_cast<int>(char_queue.size())
      });
      for (int i = 0; i < writable; ++i) {
        buf[i + 1] = char_queue.front();
        char_queue.pop();
      }
      UartWriteRegisterN(0, rpi::UART_THR, buf, writable);
      txlvl -= writable;
    }
  };

  while (1) {
    int request = ReceiveValue(request_tid, message);
    if (request <= 0) continue;

    switch (message.header) {
      case UART_MESSAGE::TX_NOTIFIER: { // notifier
        if (char_queue.empty()) {
          notifier_is_parked = true;
          break;
        }
        try_write();
        if (!char_queue.empty()) {
          notifier_is_parked = false;
          ReplyValue(notifier, UART_REPLY::OK);
        } else {
          notifier_is_parked = true;
        }
        break;
      }
      case UART_MESSAGE::PUTC: { // putc
        char_queue.push(message.data_as<char>());
        ReplyValue(request_tid, UART_REPLY::OK);
        try_write();
        if (!char_queue.empty() && notifier_is_parked) {
          notifier_is_parked = false;
          ReplyValue(notifier, UART_REPLY::OK);
        }
        break;
      }
      case UART_MESSAGE::PUTS: { // puts
        auto &dat = message.data_as<uart_puts_message>();
        for (size_t i = 0; i < dat.data_size; ++i) {
          char_queue.push(dat.data[i]);
        }
        ReplyValue(request_tid, UART_REPLY::OK);
        try_write();
        if (!char_queue.empty() && notifier_is_parked) {
          notifier_is_parked = false;
          ReplyValue(notifier, UART_REPLY::OK);
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
