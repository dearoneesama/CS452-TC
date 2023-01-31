#include "tasking.hpp"
#include "user.hpp"

using namespace kernel;

task_descriptor *task_manager::new_task(tid_t parent_tid, size_t parent_generation, priority_t priority, task_state_t state) {
  auto *task = allocator.allocate();
  auto i = allocator.index_of(task);
  // tid starts at 2 for tasks
  task->tid = i + STARTING_TASK_TID;
  task->parent_tid = parent_tid;
  task->priority = priority;
  task->state = state;

  task_reuse_statuses[i].gen++;
  task_reuse_statuses[i].parent_gen = parent_generation;
  task_reuse_statuses[i].free = 0;
  // point to end of stack space since it grows backward
  task->context.stack_pointer = reinterpret_cast<uint64_t>(stack_buff + (i + 1) * TASK_STACK_SIZE);
  return task;
}

task_descriptor *task_manager::get_task() {
  return ready.size() ? &ready.pop() : nullptr;
}

void task_manager::ready_push(task_descriptor *task) {
  task->state = task_state_t::Ready;
  ready.push(*task, task->priority);
}

void task_manager::k_create(task_descriptor *curr_task) {
  // when the current task calls this syscall
  // x0 holds priority, and x1 holds the function pointer

  // check priority
  auto priority = static_cast<priority_t>(curr_task->context.registers[0]);
  if (!(PRIORITY_L0 <= priority && priority < PRIORITY_UNDEFINED)) {
    curr_task->context.registers[0] = -1;
    ready_push(curr_task);
    return;
  }

  // check whether one can still allocate new task
  if (allocator.num_allocated() == allocator.capacity) {
    curr_task->context.registers[0] = -2;
    ready_push(curr_task);
    return;
  }
  auto *new_task = this->new_task(curr_task->tid, task_reuse_statuses[curr_task->tid - 2].gen, priority);
  new_task->context.exception_lr = reinterpret_cast<uint64_t>(task_wrapper);
  new_task->context.registers[0] = curr_task->context.registers[1]; // the actual function
  new_task->context.spsr = 0;

  // return the new task's tid to the current task
  curr_task->context.registers[0] = new_task->tid;

  // good question: if they have the same priority, what should the order be?
  ready_push(curr_task);
  ready_push(new_task);
}

void task_manager::k_my_tid(task_descriptor *curr_task) {
  curr_task->context.registers[0] = curr_task->tid;
  ready_push(curr_task);
}

void task_manager::k_my_parent_tid(task_descriptor *curr_task) {
  tid_t parent_tid = curr_task->parent_tid;

  if (parent_tid != KERNEL_TID) {
    // the parent task might have exited
    size_t parent_index = parent_tid - STARTING_TASK_TID;
    size_t curr_index = curr_task->tid - STARTING_TASK_TID;
    // if the parent descriptor is free, or if the generations do not match, then it must be that
    // the parent task has already exited
    if (task_reuse_statuses[parent_index].free || task_reuse_statuses[parent_index].gen != task_reuse_statuses[curr_index].parent_gen) {
      parent_tid |= EXITED_PARENT_MASK;
    }
  }

  curr_task->context.registers[0] = parent_tid;
  ready_push(curr_task);
}

void task_manager::k_yield(task_descriptor *curr_task) {
  ready_push(curr_task);
}

void task_manager::k_exit(task_descriptor *curr_task) {
  task_reuse_statuses[curr_task->tid - STARTING_TASK_TID].free = 1;
  curr_task->state = task_state_t::Free; // not needed but for good measures
  allocator.free(curr_task);
}
