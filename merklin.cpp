#include "merklin.hpp"
#include "kstddefs.hpp"
#include "user_syscall_typed.hpp"
#include "rpi.hpp"
#include <etl/queue.h>

namespace merklin {

void merklin_rxnotifer() {
  tid_t server = MyParentTid();
  while (1) {
    AwaitEvent(events_t::UART_R1);
    SendValue(server, 'n', null_reply);
  }
}

void merklin_ctsnotifier() {
  tid_t server = MyParentTid();
  int value;
  while (1) {
    value = AwaitEvent(events_t::CTS_1);
    if ((value & 1) == 1) {
      SendValue(server, 'c', null_reply);
    }
  }
}

void merklin_txnotifier() {
  tid_t server = MyParentTid();
  while (1) {
    AwaitEvent(events_t::UART_T1);
    SendValue(server, 't', null_reply);
  }
}

void merklin_rxserver() {
  RegisterAs(MERK_RX_SERVER_NAME);
  tid_t notifier = Create(priority_t::PRIORITY_L1, merklin_rxnotifer);
  tid_t request_tid;
  char message[1];
  etl::queue<char, 10> char_queue;
  etl::queue<tid_t, 10> requester_queue;
  char reply = 'o';
  // bool is_first_notifier_msg = true;

  while (1) {
    int request = ReceiveValue(request_tid, message);
    if (request <= 0) continue;
    switch (message[0]) {
      case 'n': { // notifier
        char_queue.push(UartReadRegister(1, rpi::UART_RHR));
        ReplyValue(notifier, reply);
        if (!requester_queue.empty()) {
          ReplyValue(requester_queue.front(), char_queue.front());
          requester_queue.pop();
          char_queue.pop();
        }
        break;
      }
      case 'g': { // getc
        requester_queue.push(request_tid);
        if (!char_queue.empty()) {
          ReplyValue(requester_queue.front(), char_queue.front());
          requester_queue.pop();
          char_queue.pop();
        }
        break;
      }
      default: break;
    }
  }
}

void merklin_txserver() {
  RegisterAs(MERK_TX_SERVER_NAME);
  tid_t tx_notifier = Create(priority_t::PRIORITY_L1, merklin_txnotifier);
  tid_t cts_notifier = Create(priority_t::PRIORITY_L1, merklin_ctsnotifier);
  tid_t request_tid;
  char message[2];
  etl::queue<char, 512> char_queue;
  char reply = 'o';
  bool cts_gone_back_up = true;
  bool cts = true;
  bool tx_up = true;

  while (1) {
    int request = ReceiveValue(request_tid, message);
    if (request <= 0) continue;

    switch (message[0]) {
      case 'p': { // putc
        char_queue.push(message[1]);
        ReplyValue(request_tid, reply);
        break;
      }
      case 't': { // tx notifier
        tx_up = true;
        break;
      }
      case 'c': { // cts notifier
        cts_gone_back_up = !cts_gone_back_up;
        if (cts_gone_back_up) {
          cts = true;
        }
        ReplyValue(cts_notifier, reply);
        break;
      }
      default: break;
    }

    // check whether or not we can write
    if (tx_up && cts && cts_gone_back_up && !char_queue.empty()) {
      UartWriteRegister(1, rpi::UART_THR, char_queue.front());
      char_queue.pop();
      tx_up = false;
      cts = false;

      ReplyValue(tx_notifier, reply);
    }
  }
}

void init_tasks() {
  Create(priority_t::PRIORITY_L1, merklin_txserver);
  Create(priority_t::PRIORITY_L1, merklin_rxserver);
}

}
