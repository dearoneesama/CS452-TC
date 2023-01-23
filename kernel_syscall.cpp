#include "kernel_syscall.hpp"

namespace {
void push_back_to_ready_lists(task_descriptor* task, ready_lists* rl) {
  task->state = Ready;
  rl->push(task);
}

void push_back_to_free_list(task_descriptor* task, task_list* free_list) {
  task->state = Free;
  free_list->push(task);
}
}  // namespace

namespace kernel {
void kCreate(task_descriptor* current_task, task_list* free_list, ready_lists* rl) {
  if (!(P0 <= current_task->context.registers[0] && current_task->context.registers[0] <= P5)) {
    // invalid priority
    current_task->context.registers[0] = -1;
    push_back_to_ready_lists(current_task, rl);
    return;
  }

  if (free_list->empty()) {
    // ran out of task descriptors
    current_task->context.registers[0] = -2;
    push_back_to_ready_lists(current_task, rl);
    return;
  }

  // when the current task calls this syscall
  // x0 holds priority, and x1 holds the function pointer
  task_descriptor* new_task = free_list->pop();

  new_task->parent_tid = current_task->tid;
  new_task->priority = (priority_t)current_task->context.registers[0];
  new_task->context.exception_lr = current_task->context.registers[1];
  new_task->context.spsr = 0;

  // return the new task's tid to the current task
  current_task->context.registers[0] = new_task->tid;

  // good question: if they have the same priority, what should the order be?
  push_back_to_ready_lists(current_task, rl);
  push_back_to_ready_lists(new_task, rl);
}

void kMyTid(task_descriptor* current_task, ready_lists* rl) {
  current_task->context.registers[0] = current_task->tid;
  push_back_to_ready_lists(current_task, rl);
}

void kMyParentTid(task_descriptor* current_task, ready_lists* rl) {
  current_task->context.registers[0] = current_task->parent_tid;
  push_back_to_ready_lists(current_task, rl);
}

void kYield(task_descriptor* current_task, ready_lists* rl) { push_back_to_ready_lists(current_task, rl); }

void kExit(task_descriptor* current_task, task_list* free_list) { push_back_to_free_list(current_task, free_list); }
}  // namespace kernel
