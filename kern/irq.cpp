#include "irq.hpp"

// gic and irq things
namespace {
// broadcom document
static const uint32_t GIC_400_BASE = 0xff840000;
// https://developer.arm.com/documentation/ddi0471/b/programmers-model/gic-400-register-map?lang=en
static const uint32_t GIC_400_DIST_BASE = GIC_400_BASE + 0x1000;
static const uint32_t GIC_400_CPU_INTERFACE_BASE = GIC_400_BASE + 0x2000;

static const uint32_t VIDEO_CORE_BASE = 96;
static const uint32_t SYSTEM_TIMER_C1 = VIDEO_CORE_BASE + 1;
static const uint32_t GPIO_IRQ = VIDEO_CORE_BASE + 49;
static const uint32_t INTERRUPT_ID_MASK = 0x3FF; // bits[0:9]

// ARM GIC spec page 93
void set_enable_irq_by_id(uint32_t m) {
  uint32_t n = m >> 5; // m DIV 32
  uint32_t offset = m & (32 - 1); // m MOD 32
  uint32_t* gicd_isenabler = reinterpret_cast<uint32_t *>(GIC_400_DIST_BASE + (0x100 + (4 * n)));
  *gicd_isenabler = (1 << offset);
}

// ARM GIC spec page 107
void set_target_irq_by_id(uint32_t m) {
  uint32_t n = m >> 2; // m DIV 4
  uint32_t byte_offset = m & (4 - 1); // m MOD 4
  uint32_t* gicd_itargetsr = reinterpret_cast<uint32_t *>(GIC_400_DIST_BASE + (0x800 + (4 * n)));
  uint32_t forward_to_cpu_0 = 1;
  *gicd_itargetsr = (forward_to_cpu_0 << (8 * byte_offset));
}

}; // namespace

namespace irq {

void initialize_irq() {
  set_enable_irq_by_id(SYSTEM_TIMER_C1);
  set_target_irq_by_id(SYSTEM_TIMER_C1);

  set_enable_irq_by_id(GPIO_IRQ);
  set_target_irq_by_id(GPIO_IRQ);
}

// ARM GIC spec page 76 and 135
uint32_t read_interrupt_iar() {
  return *reinterpret_cast<uint32_t*>(GIC_400_CPU_INTERFACE_BASE + 0xc);
}

uint32_t get_irq_id(uint32_t iar) {
  return iar & INTERRUPT_ID_MASK;
}

void end_interrupt(uint32_t iar) {
  *reinterpret_cast<uint32_t*>(GIC_400_CPU_INTERFACE_BASE + 0x10) = iar;
}

bool is_timer_interrupt(uint32_t irq_id) {
  return irq_id == SYSTEM_TIMER_C1;
}

bool is_gpio_interrupt(uint32_t irq_id) {
  return irq_id == GPIO_IRQ;
}

} // namespace irq
