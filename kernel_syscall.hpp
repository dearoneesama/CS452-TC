#pragma once
#include "ready_lists.hpp"
#include "task_descriptor.hpp"

namespace kernel {

void kCreate(task_descriptor* current_task, task_list* free_list, ready_lists* rl);

void kMyTid(task_descriptor* current_task, ready_lists* rl);

void kMyParentTid(task_descriptor* current_task, ready_lists* rl);

void kYield(task_descriptor* current_task, ready_lists* rl);

void kExit(task_descriptor* current_task, task_list* free_list);

}  // namespace kernel
