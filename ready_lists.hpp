#pragma once

#include "task_descriptor.hpp"
#include "task_list.hpp"
#include "priority.hpp"
#include <stddef.h>

struct ready_lists {
  ready_lists() : lists{}, size{0} {}

  void push(task_descriptor* td) {
    // maybe assert that td.priority is not Undef here
    lists[td->priority].push(td);
    ++size;
  }

  task_descriptor* pop() {
    // maybe assert that size != 0 here
    for (int i = 0; i < NUM_PRIORITY; ++i) {
      if (!(lists[i].empty())) {
        --size;
        return lists[i].pop();
      }
    }
    return nullptr;
  }

  bool empty() {
    return size == 0;
  }

  task_list lists[NUM_PRIORITY];
  size_t size;
};
