#include "user_syscall_typed.hpp"

int Getc(int tid, int) {
  char message = 'g';
  char reply;
  SendValue(tid, message, reply);
  return reply;
}

int Putc(int tid, int, char c) {
  char message[2] = { 'p', c };
  char reply;
  SendValue(tid, message, reply);
  if (reply == 'o') {
    return 0;
  }
  return -1;
}

int Puts(int tid, int, const char *s, size_t len) {
  for (size_t i = 0; i < len; i++) {
    if (Putc(tid, 1, s[i]) < 0) {
      return -1;
    }
  }
  return 0;
}

int Puts(int tid, int channel, const char *s) {
  size_t len = 0;
  while (s[len++]);
  return Puts(tid, channel, s, len);
}
