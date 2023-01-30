#pragma once

#include "containers.hpp"
#include "kstddefs.hpp"

namespace kernel {
  enum class task_state_t : int {
    Free,    // in free list
    Active,  // currently running
    Ready,   // can be run
    Blocked, // waiting on something
    SendWait,    // sender waiting on receiver
    ReceiveWait, // receiver waiting on sender
    ReplyWait,   // sender waiting on reply
  };

  // Align everything to 64bit for easier time on the assembler side
  struct context_t {
    int64_t registers[31];  // x0 to x30
    uint64_t spsr;  // saved program status reg
    uint64_t stack_pointer;
    uint64_t exception_lr;
  };

  struct task_descriptor : public troll::forward_link {
    tid_t tid = 0;
    tid_t parent_tid = 0;
    priority_t priority = PRIORITY_UNDEFINED;
    task_state_t state = task_state_t::Free;
    context_t context;  // TODO: initialize the context to point to some error function

    task_descriptor() = default;
  };

  struct task_reuse_status {
    size_t parent_gen = 0;
    size_t gen = 0;
    bool free = 1;
  };

  class task_manager {
  public:
    task_manager() = default;
    task_manager(task_manager &) = delete;

    // allocate a task. its tid is created automatically, but do not run it.
    task_descriptor *new_task(tid_t parent_tid, size_t parent_generation, priority_t priority, task_state_t state = task_state_t::Ready);
    // get a runnable stack, or nullptr
    task_descriptor *get_task();
    void ready_push(task_descriptor *task);

    // kernel syscalls
    void k_create(task_descriptor *curr_task);
    void k_my_tid(task_descriptor *curr_task);
    void k_my_parent_tid(task_descriptor *curr_task);
    void k_yield(task_descriptor *curr_task);
    void k_send(task_descriptor *curr_task);
    void k_receive(task_descriptor *curr_task);
    void k_reply(task_descriptor *curr_task);
    void k_exit(task_descriptor *curr_task);

  private:
    // reserved memory for stacks
    alignas(SP_ALIGNMENT) char stack_buff[TASK_STACK_SIZE * MAX_NUM_TASKS];
    // free list of task descriptors
    troll::free_list<task_descriptor, MAX_NUM_TASKS> allocator;
    // ready tasks
    troll::intrusive_priority_scheduling_queue<task_descriptor, NUM_PRIORITIES> ready;
    // this keeps track of the reuse stats of each task descriptor
    // useful for determining whether a parent task has already exited
    task_reuse_status task_reuse_statuses[MAX_NUM_TASKS];
    // mailboxes for message passing
    troll::queue<task_descriptor> mailboxes[MAX_NUM_TASKS];
  };
}  // namespace kernel
