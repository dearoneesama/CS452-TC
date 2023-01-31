#pragma once

/**
 * With the task wrapper, a user task does not need to explicitly call Exit
*/
void task_wrapper(void (*the_task)());
void first_user_task();
