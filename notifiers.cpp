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
    value = AwaitEvent(0);
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
