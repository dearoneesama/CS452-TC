#pragma once

/**
 * With the task wrapper, a user task does not need to explicitly call Exit
*/
void task_wrapper(void (*the_task)());

namespace k1 {
  void first_user_task();
}

namespace k2 {
  void test_user_task();
  void rps_first_user_task();
  void perf_main_task();
  void first_user_task();
}

namespace k3 {
  void first_user_task();
}
