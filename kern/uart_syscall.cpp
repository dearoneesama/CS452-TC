#include "user_syscall_typed.hpp"
#include "servers.hpp"
#include "../generic/utils.hpp"

int Getc(int tid, int) {
  char reply;
  SendValue(tid, UART_MESSAGE::GETC, reply);
  return reply;
}

int Putc(int tid, int, char c) {
  UART_REPLY reply;
  SendValue(tid, utils::enumed_class {
    UART_MESSAGE::PUTC,
    uart_putc_message {c},
  }, reply);
  if (reply == UART_REPLY::OK) {
    return 0;
  }
  return -1;
}

int Puts(int tid, int, const char *s, size_t len) {
  UART_REPLY reply;
  utils::enumed_class<UART_MESSAGE, uart_puts_message> msg;
  msg.header = UART_MESSAGE::PUTS;
  msg.data.data_size = len;
  memcpy(msg.data.data, s, len);
  auto msglen = reinterpret_cast<uintptr_t>(&msg.data.data[len]) - reinterpret_cast<uintptr_t>(&msg);
  SendValue(tid, msg, msglen, reply);
  if (reply == UART_REPLY::OK) {
    return 0;
  }
  return -1;
}

int Puts(int tid, int channel, const char *s) {
  size_t len = 0;
  while (s[len++]);
  return Puts(tid, channel, s, len);
}
