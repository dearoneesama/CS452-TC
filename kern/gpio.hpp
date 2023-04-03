#pragma once

#include "kstddefs.hpp"

namespace gpio {

class uart_interrupt_state {
public:
  void disable_rx(size_t uart_channel);
  void disable_tx(size_t uart_channel);
  void enable_rx(size_t uart_channel);
  void enable_tx(size_t uart_channel);
  void enable_modem_interrupt();
  void disable_modem_interrupt();
  char &get_ier(size_t uart_channel);
private:
  char ier0 = 0;
  char ier1 = 0;
};

enum class GPIO_IRQ_TYPE : char {
  NONE = 0b1,
  RECEIVER_LINE_STATUS = 0b110,
  RX_TIMEOUT = 0b1100,
  RHR = 0b100,
  THR = 0b10,
  MODEM_STATUS = 0b0,
  IO_PINS = 0b1110,
  XOFF = 0b10000,
  CTS = 0b100000,
};

GPIO_IRQ_TYPE get_interrupt_type(size_t spi_channel, size_t uart_channel);

}
