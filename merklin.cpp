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

#if IS_TRACK_A
// work around for track A
void delay_notifier() {
  tid_t server = MyParentTid();
  auto clock_server = TaskFinder("clock_server");
  while (1) {
    SendValue(server, 'd', null_reply);
    Delay(clock_server(), 15); // 150ms delay
  }
}
#endif

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

#if IS_TRACK_A
  tid_t d_notifier = Create(priority_t::PRIORITY_L2, delay_notifier);
  bool can_send = false;
#else
  tid_t cts_notifier = Create(priority_t::PRIORITY_L1, merklin_ctsnotifier);
  bool cts_gone_back_up = true;
  bool cts = true;
#endif
  tid_t request_tid;
  char message[2];
  etl::queue<char, 512> char_queue;
  char reply = 'o';
  bool tx_up = true;

  while (1) {
    int request = ReceiveValue(request_tid, message);
    if (request <= 0) continue;

    switch (message[0]) {
#if IS_TRACK_A
      case 'd': { // delay
        can_send = true;
        break;
      }
#else
      case 'c': { // cts notifier
        cts_gone_back_up = !cts_gone_back_up;
        if (cts_gone_back_up) {
          cts = true;
        }
        ReplyValue(cts_notifier, reply);
        break;
      }
#endif
      case 'p': { // putc
        char_queue.push(message[1]);
        ReplyValue(request_tid, reply);
        break;
      }
      case 't': { // tx notifier
        tx_up = true;
        break;
      }
      default: break;
    }

#if IS_TRACK_A
    if (tx_up && can_send && !char_queue.empty()) {
      UartWriteRegister(1, rpi::UART_THR, char_queue.front());
      char_queue.pop();
      tx_up = false;
      can_send = false;
      ReplyValue(tx_notifier, reply);
      ReplyValue(d_notifier, reply);
    }
#else
    // check whether or not we can write
    if (tx_up && cts && cts_gone_back_up && !char_queue.empty()) {
      UartWriteRegister(1, rpi::UART_THR, char_queue.front());
      char_queue.pop();
      tx_up = false;
#if DEBUG_PI
#else
      cts = false;
#endif
      ReplyValue(tx_notifier, reply);
    }
#endif
  }
}

void init_tasks() {
  Create(priority_t::PRIORITY_L1, merklin_txserver);
  Create(priority_t::PRIORITY_L1, merklin_rxserver);
}

}
