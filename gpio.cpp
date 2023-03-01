#include "gpio.hpp"
#include "rpi.hpp"

namespace {

static constexpr char IIR_MASK = 0b00111111;

static constexpr char ENABLE_RX_MASK = 0b1;
static constexpr char ENABLE_TX_MASK = 0b10;
static constexpr char ENABLE_TX_AND_CTS_MASK = ENABLE_TX_MASK | 0b1000;
};

namespace gpio {

char &uart_interrupt_state::get_ier(size_t uart_channel) {
  return uart_channel == 0 ? ier0 : ier1;
}

void uart_interrupt_state::disable_rx(size_t uart_channel) {
  char &ier = get_ier(uart_channel);
  ier &= ~ENABLE_RX_MASK;
  uart_write_register(0, uart_channel, rpi::UART_IER, ier);
}

void uart_interrupt_state::disable_tx(size_t uart_channel) {
  char &ier = get_ier(uart_channel);
  ier &= ~ENABLE_TX_MASK;
  uart_write_register(0, uart_channel, rpi::UART_IER, ier);
}

void uart_interrupt_state::enable_rx(size_t uart_channel) {
  char &ier = get_ier(uart_channel);
  ier |= ENABLE_RX_MASK;
  uart_write_register(0, uart_channel, rpi::UART_IER, ier);
}

void uart_interrupt_state::enable_tx(size_t uart_channel) {
  char &ier = get_ier(uart_channel);
  ier |= uart_channel == 0 ? ENABLE_TX_MASK : ENABLE_TX_AND_CTS_MASK;
  uart_write_register(0, uart_channel, rpi::UART_IER, ier);
}

GPIO_IRQ_TYPE get_interrupt_type(size_t spi_channel, size_t uart_channel) {
  char iir = uart_read_register(spi_channel, uart_channel, rpi::UART_IIR);
  return static_cast<GPIO_IRQ_TYPE>(iir & IIR_MASK);
}

}
