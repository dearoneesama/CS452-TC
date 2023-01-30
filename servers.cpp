#include "servers.hpp"
#include "kstddefs.hpp"
#include "user_syscall.h"

namespace {
bool equal_string(const char* s1, const char* s2, size_t n) {
  for (size_t i = 0; i < n; ++i) {
    if (*(s1++) != *(s2++)) {
      return false;
    }
  }
  return true;
}

void copy_to(char* dest, const char* src, size_t n) {
  for (size_t i = 0; i < n; ++i) {
    *(dest++) = *(src++);
  }
}
} // namespace

void nameserver() {
  constexpr char name_length = 20;
  constexpr char buffer_length = name_length + 1;
  char names[MAX_NUM_TASKS][name_length];
  char name_lengths[MAX_NUM_TASKS];

  for (size_t i = 0; i < MAX_NUM_TASKS; ++i) {
    name_lengths[i] = -1; // -1 means that the name is not registered
  }

  char tid_buffer[4];

  char buffer[buffer_length];
  tid_t request_tid;
  while (1) {
    int request = Receive((int*)&request_tid, buffer, buffer_length);
    if (request > 0) {
      switch (buffer[0]) {
      case 'r': { // register
        char request_name_length = request > name_length + 1 ? name_length : request - 1;
        const char *request_name = buffer + 1;
        for (size_t i = 0; i < MAX_NUM_TASKS; ++i) {
          if (name_lengths[i] == request_name_length && equal_string(names[i], request_name, request_name_length)) {
            name_lengths[i] = -1;
            break;
          }
        }
        size_t index = request_tid - STARTING_TASK_TID;
        copy_to(names[index], request_name, request_name_length);
        name_lengths[index] = request_name_length;
        Reply(request_tid, "1", 1);
        break;
      }
      case 'w': { // who is
        char request_name_length = request > name_length + 1 ? name_length : request - 1;
        const char *request_name = buffer + 1;
        tid_t target_tid = 0;
        for (size_t i = 0; i < MAX_NUM_TASKS; ++i) {
          if (name_lengths[i] == request_name_length && equal_string(names[i], request_name, request_name_length)) {
            target_tid = i + STARTING_TASK_TID;
          }
        }
        if (target_tid >= STARTING_TASK_TID) {
          tid_buffer[0] = target_tid;
          tid_buffer[1] = target_tid >> 8;
          tid_buffer[2] = target_tid >> 16;
          tid_buffer[3] = target_tid >> 24;
          Reply(request_tid, tid_buffer, 4);
        } else {
          Reply(request_tid, 0, 0); // no such name
        }
        break;
      }
      default:
        break;
      }
    }
  }
}
