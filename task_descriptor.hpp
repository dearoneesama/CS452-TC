#pragma once

#include <stdint.h>
#include "priority.hpp"
#include "context.hpp"

enum task_state_t {
  Active,  // currently running
  Ready,   // can be run
  Blocked, // waiting on something
  Free,    // in free list
};

using tid_t = uint32_t;

struct task_descriptor {
  tid_t tid;
  tid_t parent_tid;
  priority_t priority;
  task_state_t state;
  context_t context; // TODO: initialize the context to point to some error function
  task_descriptor* next;

  task_descriptor() : task_descriptor{0, 0, Undef, Free} {}

  task_descriptor(tid_t tid, tid_t parent_tid, priority_t priority, task_state_t state)
    : tid{tid}, parent_tid{parent_tid}, priority{priority}, state{state}, context{}, next{nullptr} {}
};
