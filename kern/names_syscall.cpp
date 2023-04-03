#include "user_syscall_typed.hpp"

int RegisterAs(const char* name) {
  if (!name) {
    return -2;
  }

  char request_buffer[33];
  request_buffer[0] = 'r';
  size_t request_length = 1;
  while (*name) {
    request_buffer[request_length++] = *(name++);
  }
  request_buffer[request_length++] = '\0';
  char reply_buffer[1];
  int reply_len = SendValue(3, request_buffer, request_length, reply_buffer);
  if (reply_len == 1 && reply_buffer[0] == '1') {
    return 0;
  }
  if (reply_len < 0) {
    return -1;
  }
  return -2; // failed for some reason
}

int WhoIs(const char* name) {
  if (!name) {
    return -2;
  }

  char request_buffer[33];
  request_buffer[0] = 'w';
  size_t request_length = 1;
  while (*name) {
    request_buffer[request_length++] = *(name++);
  }
  request_buffer[request_length++] = '\0';
  char reply_buffer[4];
  int reply_len = SendValue(3, request_buffer, request_length, reply_buffer);
  if (reply_len < 0) {
    return -1;
  }
  if (reply_len == 0) {
    return 0; // no task with this name
  }
  if (reply_len == 4) {
    tid_t the_tid = reply_buffer[0] + (reply_buffer[1] << 8) + (reply_buffer[2] << 16) + (reply_buffer[3] << 24);
    return the_tid;
  }
  return -2; // failed for some reason
}
