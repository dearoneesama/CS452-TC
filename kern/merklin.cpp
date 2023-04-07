#include "merklin.hpp"
#include "kstddefs.hpp"
#include "user_syscall_typed.hpp"
#include "rpi.hpp"
#include "servers.hpp"
#include "../generic/utils.hpp"
#include <etl/queue.h>

namespace merklin {

void merklin_rxnotifer() {
  tid_t server = MyParentTid();
  while (1) {
    AwaitEvent(events_t::UART_R1);
    SendValue(server, UART_MESSAGE::RX_NOTIFIER, null_reply);
  }
}

#if NO_CTS
// work around for track A
void delay_notifier() {
  tid_t server = MyParentTid();
  auto clock_server = TaskFinder("clock_server");
  while (1) {
    SendValue(server, UART_MESSAGE::DELAY_NOTIFIER, null_reply);
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
      SendValue(server, UART_MESSAGE::CTS_NOTIFIER, null_reply);
    }
  }
}

void merklin_txnotifier() {
  tid_t server = MyParentTid();
  while (1) {
    AwaitEvent(events_t::UART_T1);
    SendValue(server, UART_MESSAGE::TX_NOTIFIER, null_reply);
  }
}

void merklin_rxserver() {
  RegisterAs(MERK_RX_SERVER_NAME);
  tid_t notifier = Create(priority_t::PRIORITY_L1, merklin_rxnotifer);
  tid_t request_tid;
  UART_MESSAGE message;
  etl::queue<char, 10> char_queue;
  etl::queue<tid_t, 10> requester_queue;
  // bool is_first_notifier_msg = true;

  while (1) {
    int request = ReceiveValue(request_tid, message);
    if (request <= 0) continue;
    switch (message) {
      case UART_MESSAGE::RX_NOTIFIER: { // notifier
        char_queue.push(UartReadRegister(1, rpi::UART_RHR));
        ReplyValue(notifier, UART_REPLY::OK);
        if (!requester_queue.empty()) {
          ReplyValue(requester_queue.front(), char_queue.front());
          requester_queue.pop();
          char_queue.pop();
        }
        break;
      }
      case UART_MESSAGE::GETC: { // getc
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

#if NO_CTS
  tid_t d_notifier = Create(priority_t::PRIORITY_L2, delay_notifier);
  bool can_send = false;
#else
  tid_t cts_notifier = Create(priority_t::PRIORITY_L1, merklin_ctsnotifier);
  bool cts_gone_back_up = true;
  bool cts = true;
#endif
  tid_t request_tid;
  utils::enumed_class<UART_MESSAGE, char[128]> message;
  etl::queue<char, 512> char_queue;
  bool tx_up = true;

  while (1) {
    int request = ReceiveValue(request_tid, message);
    if (request <= 0) continue;

    switch (message.header) {
#if NO_CTS
      case UART_MESSAGE::DELAY_NOTIFIER: { // delay
        can_send = true;
        break;
      }
#else
      case UART_MESSAGE::CTS_NOTIFIER: { // cts notifier
        cts_gone_back_up = !cts_gone_back_up;
        if (cts_gone_back_up) {
          cts = true;
        }
        ReplyValue(cts_notifier, UART_REPLY::OK);
        break;
      }
#endif
      case UART_MESSAGE::PUTC: { // putc
        char_queue.push(message.data_as<char>());
        ReplyValue(request_tid, UART_REPLY::OK);
        break;
      }
      case UART_MESSAGE::TX_NOTIFIER: { // tx notifier
        tx_up = true;
        break;
      }
      default: break;
    }

#if NO_CTS
    if (tx_up && can_send && !char_queue.empty()) {
      UartWriteRegister(1, rpi::UART_THR, char_queue.front());
      char_queue.pop();
      tx_up = false;
      can_send = false;
      ReplyValue(tx_notifier, UART_REPLY::OK);
      ReplyValue(d_notifier, UART_REPLY::OK);
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
      ReplyValue(tx_notifier, UART_REPLY::OK);
    }
#endif
  }
}

void init_tasks() {
  Create(priority_t::PRIORITY_L1, merklin_txserver);
  Create(priority_t::PRIORITY_L1, merklin_rxserver);
}

}
