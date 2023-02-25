#include "kernel.hpp"
#include "rpi.hpp"
#include "tasking.hpp"
#include "user.hpp"
#include "timer.hpp"
#include "format.hpp"
#include "irq.include"

void initialize() {
  // run static/global constructors manually
  using constructor_t = void (*)();
  extern constructor_t __init_array_start[], __init_array_end[];
  for (auto* fn = __init_array_start; fn < __init_array_end; (*fn++)())
    ;
}

// bits[0:24] hold N in svc N
#define ESR_MASK 0x1FFFFFF

uint32_t calculate_elapsed_time(uint32_t start_time, uint32_t end_time) {
  return end_time >= start_time ? end_time - start_time : start_time + ~end_time;
}

int main() {
  initialize();
  init_gpio();
  init_spi(0);
  init_uart(0);

  // sets up the vector exception table
  kernel::initialize();
  kernel::enable_bcache();
  kernel::timer timer;
  timer.initialize();
  kernel::task_manager task_manager;
  kernel::context_t kernel_context;

  uart_puts(0, 0, SC_CLRSCR, LEN_LITERAL(SC_CLRSCR));

  // spawn first task
  auto *current_task = task_manager.new_task(KERNEL_TID, 1, PRIORITY_L2);
  current_task->context.registers[0] = reinterpret_cast<int64_t>(k3::first_user_task);
  current_task->context.exception_lr = reinterpret_cast<uint64_t>(task_wrapper);
  task_manager.ready_push(current_task);

  uint64_t total_ticks = 0, kernel_ticks = 0, idle_ticks = 0;

  uint32_t start_time = timer.read_current_tick();
  uint32_t end_time, elapsed_time;
  uint32_t esr_el1, request;
  while ((current_task = task_manager.get_task())) {
    current_task->state = kernel::task_state_t::Active;

    end_time = timer.read_current_tick();
    elapsed_time = calculate_elapsed_time(start_time, end_time);
    total_ticks += elapsed_time;
    kernel_ticks += elapsed_time;

    start_time = end_time;
    esr_el1 = kernel::activate_task(&kernel_context, current_task);
    end_time = timer.read_current_tick();

    elapsed_time = calculate_elapsed_time(start_time, end_time);
    total_ticks += elapsed_time;

    start_time = end_time;

    request = esr_el1 & ESR_MASK;

    switch (request) {
      case SYSCALLN_CREATE: {
        task_manager.k_create(current_task);
        break;
      }
      case SYSCALLN_MYTID: {
        task_manager.k_my_tid(current_task);
        break;
      }
      case SYSCALLN_MYPARENTTID: {
        task_manager.k_my_parent_tid(current_task);
        break;
      }
      case SYSCALLN_YIELD: {
        task_manager.k_yield(current_task);
        break;
      }
      case SYSCALLN_SEND: {
        task_manager.k_send(current_task);
        break;
      }
      case SYSCALLN_RECEIVE: {
        task_manager.k_receive(current_task);
        break;
      }
      case SYSCALLN_REPLY: {
        task_manager.k_reply(current_task);
        break;
      }
      case SYSCALLN_AWAITEVENT: {
        task_manager.k_await_event(current_task);
        break;
      }
      case SYSCALLN_EXIT: {
        task_manager.k_exit(current_task);
        break;
      }
      case SYSCALLN_TIMEDISTRIBUTION: {
        // note: only the idle task should call this
        time_distribution_t* time_dist = (time_distribution_t*) current_task->context.registers[0];
        time_dist->kernel_ticks = kernel_ticks;
        time_dist->idle_ticks = idle_ticks;
        time_dist->total_ticks = total_ticks;
        task_manager.ready_push(current_task);
        break;
      }
      case SYSCALLN_SAVETHEPLANET: {
        // note: only the idle task should call this
        // also note that userspace is NOT able to call wfi
        // see: https://patchwork.kernel.org/project/linux-arm-kernel/patch/20180807093326.5090-1-marc.zyngier@arm.com/
        asm volatile("dsb ish");
        asm volatile("wfi");
        end_time = timer.read_current_tick();
        elapsed_time = calculate_elapsed_time(start_time, end_time);

        total_ticks += elapsed_time;
        idle_ticks += elapsed_time;

        start_time = end_time;
        kernel::handle_interrupt(task_manager, timer);
        task_manager.ready_push(current_task);
        break;
      }
      case IRQ: {
        kernel::handle_interrupt(task_manager, timer);
        task_manager.ready_push(current_task);
        break;
      }
      // only for benchmarking
      case SYSBENCHMARK_ICACHE: {
        task_manager.kp_icache(current_task);
        break;
      }
      case SYSBENCHMARK_BCACHE: {
        task_manager.kp_bcache(current_task);
        break;
      }
      case SYSBENCHMARK_DCACHE: {
        task_manager.kp_dcache(current_task);
        break;
      }
      default: {
        // in case the kernel does not recognize the request, print it and return
        uart_puts(0, 0, "Got request: ", 13);
        for (int i = 0; i < 32; ++i) {
          uart_putc(0, 0, '0' + (esr_el1 & 1));
          esr_el1 >>= 1;
        }
        uart_puts(0, 0, "\r\n", 2);
        return 0;
      }
    }
  }
  uart_puts(0, 0, "Finished processing all tasks!\r\n", 32);
  return 0;
}
