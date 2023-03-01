#include "user_syscall.h"

int Getc(int tid, int channel) {
  char message = 'g';
  char reply;
  Send(tid, &message, 1, &reply, 1);
  return reply;
}

int Putc(int tid, int channel, char c) {
  char message[2] = { 'p', c };
  char reply;
  Send(tid, message, 2, &reply, 1);
  if (reply == 'o') {
    return 0;
  }
  return -1;
}
