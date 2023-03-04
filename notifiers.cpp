#include "notifiers.hpp"
#include "rpi.hpp"
#include "user_syscall_typed.hpp"
#include "servers.hpp"

void clocknotifier() {
  tid_t clock_server_tid = WhoIs("clock_server");

  if (!(STARTING_TASK_TID <= clock_server_tid && clock_server_tid <= ENDING_TASK_TID)) {
    DEBUG_LITERAL("Error querying the clock server tid from notifier...\r\n");
    return;
  }

  while (1) {
    int value = AwaitEvent(static_cast<char>(events_t::TIMER));
    if (value == 1) {
      CLOCK_REPLY reply;
      value = SendValue(clock_server_tid, CLOCK_MESSAGE::NOTIFY, reply);
      if (value == 1 && reply == CLOCK_REPLY::NOTIFY_OK) {
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
