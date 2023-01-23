#pragma once

#include "task_descriptor.hpp"

struct task_list {
  task_descriptor* head = nullptr;
  task_descriptor* tail = nullptr;

  void push(task_descriptor* td) {
    if (head == nullptr) {
      head = td;
      tail = td;
    } else {
      tail->next = td;
      tail = td;
    }
  }

  task_descriptor* pop() {
    task_descriptor* td = head;
    head = head->next;
    if (head == nullptr) {
      tail = nullptr;
    }
    td->next = nullptr;
    return td;
  }

  bool empty() {
    return head == nullptr;
  }
};
