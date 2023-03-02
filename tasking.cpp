#include "tasking.hpp"
#include "user.hpp"
#include "kernel.hpp"
#include "rpi.hpp"

using namespace kernel;

namespace {

void send_message(task_descriptor* sender, task_descriptor* receiver) {
  tid_t sender_tid = sender->tid;
  const char* src = reinterpret_cast<const char*>(sender->context.registers[1]);
  size_t srclen = sender->context.registers[2];

  tid_t* tid = reinterpret_cast<tid_t*>(receiver->context.registers[0]);
  char* dest = reinterpret_cast<char*>(receiver->context.registers[1]);
  size_t destlen = sender->context.registers[2];

  size_t n = srclen > destlen ? destlen : srclen;
  memcpy(dest, src, n);
  *tid = sender_tid;
  receiver->context.registers[0] = n;
  sender->state = task_state_t::ReplyWait;
}

void reply_message(task_descriptor* sender, task_descriptor *replier) {
  const char* src = reinterpret_cast<const char*>(replier->context.registers[1]);
  size_t srclen = replier->context.registers[2];

  char* dest = reinterpret_cast<char *>(sender->context.registers[3]);
  size_t destlen = sender->context.registers[4];

  size_t n = srclen > destlen ? destlen : srclen;
  memcpy(dest, src, n);

  replier->context.registers[0] = n;
  sender->context.registers[0] = n;
}
}; // namespace

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
  task->context.spsr = 0; // make sure to not mask irq
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
  if (!(PRIORITY_IDLE <= priority && priority < PRIORITY_UNDEFINED)) {
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

void task_manager::k_send(task_descriptor *curr_task) {
  tid_t target_tid = curr_task->context.registers[0];

  // SRR cannot be completed, a task cannot send itself a message
  if (target_tid == curr_task->tid) {
    curr_task->context.registers[0] = -2;
    ready_push(curr_task);
    return;
  }

  task_descriptor* target_task = allocator.at(target_tid - STARTING_TASK_TID);
  if (!target_task || task_reuse_statuses[target_tid - STARTING_TASK_TID].free) {
    // invalid tid, or task exited
    curr_task->context.registers[0] = -1;
    ready_push(curr_task);
    return;
  }

  if (target_task->state == task_state_t::ReceiveWait) {
    send_message(curr_task, target_task);
    ready_push(target_task);
    return;
  }

  curr_task->state = task_state_t::SendWait;
  mailboxes[target_tid - STARTING_TASK_TID].push(*curr_task);
}

void task_manager::k_receive(task_descriptor *curr_task) {
  if (!mailboxes[curr_task->tid - STARTING_TASK_TID].empty()) {
    task_descriptor *sender_task = &(mailboxes[curr_task->tid - STARTING_TASK_TID].pop());
    send_message(sender_task, curr_task);
    ready_push(curr_task);
    return;
  }

  curr_task->state = task_state_t::ReceiveWait;
}

void task_manager::k_reply(task_descriptor *curr_task) {
  tid_t sender_tid = curr_task->context.registers[0];
  task_descriptor* sender_task = allocator.at(sender_tid - STARTING_TASK_TID);
  if (!sender_task || task_reuse_statuses[sender_tid - STARTING_TASK_TID].free) {
    // invalid tid, or task exited
    curr_task->context.registers[0] = -1;
    ready_push(curr_task);
    return;
  }

  if (sender_task->state != task_state_t::ReplyWait) {
    curr_task->context.registers[0] = -2;
    ready_push(curr_task);
    return;
  }

  reply_message(sender_task, curr_task);
  ready_push(sender_task);
  ready_push(curr_task);
}

void task_manager::k_await_event(task_descriptor *curr_task, gpio::uart_interrupt_state& state) {
  events_t event_id = (events_t) (curr_task->context.registers[0]);
  // if (missed_event_queues[event_id] != 0) {
  //   curr_task->context.registers[0] = missed_event_queues[event_id];
  //   missed_event_queues[event_id] = 0;
  //   ready_push(curr_task);
  // }
  if (event_id < MAX_NUM_EVENTS) {
    event_queues[event_id].push(*curr_task);
    switch (event_id) {
      case events_t::UART_R0: {
        state.enable_rx(0);
        break;
      }
      case events_t::UART_T0: {
        state.enable_tx(0);
        break;
      }
      case events_t::UART_R1: {
        state.enable_rx(1);
        break;
      }
      case events_t::UART_T1: {
        state.enable_tx(1);
        break;
      }
      case events_t::CTS_1: {
        // this should always be enabled
        break;
      }
      default:
        break;
    }
  } else {
    curr_task->context.registers[0] = -1;
    ready_push(curr_task);
  }
}

void task_manager::wake_up_tasks_on_event(events_t event_id, int return_value) {
  auto& event_queue = event_queues[event_id];
  if (!event_queue.size()) {
    // DEBUG_LITERAL("[kernel] no task is waiting on event!\r\n");
    // missed_event_queues[event_id] = return_value;
  }
  while (event_queue.size()) {
    auto& task = event_queue.pop();
    task.context.registers[0] = return_value;
    ready_push(&task);
  }
}

void task_manager::k_uart_read(task_descriptor *curr_task) {
  int uart_channel = curr_task->context.registers[0];
  char reg = curr_task->context.registers[1];
  char data = uart_read_register(0, uart_channel, reg);
  curr_task->context.registers[0] = data;
  ready_push(curr_task);
}

void task_manager::k_uart_write(task_descriptor *curr_task) {
  int uart_channel = curr_task->context.registers[0];
  char reg = curr_task->context.registers[1];
  char data = curr_task->context.registers[2];
  uart_write_register(0, uart_channel, reg, data);
  curr_task->context.registers[0] = 0;
  ready_push(curr_task);
}

void task_manager::kp_dcache(task_descriptor *curr_task) {
  kernel::enable_dcache();
  ready_push(curr_task);
}

void task_manager::kp_bcache(task_descriptor *curr_task) {
  kernel::enable_bcache();
  ready_push(curr_task);
}

void task_manager::kp_icache(task_descriptor *curr_task) {
  kernel::enable_icache();
  ready_push(curr_task);
}
