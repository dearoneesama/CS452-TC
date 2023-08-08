#pragma once

#include <stddef.h>
#include <stdint.h>
#include <troll_util/format.hpp>

void init_gpio();
void init_spi(uint32_t channel);
void init_uart(uint32_t spiChannel);
char uart_getc(size_t spiChannel, size_t uartChannel);
void uart_putc(size_t spiChannel, size_t uartChannel, char c);
void uart_puts(size_t spiChannel, size_t uartChannel, const char* buf, size_t blen);
void uart_putui64(size_t spiChannel, size_t uartChannel, uint64_t n);
extern "C" void *memset(void *s, int c, size_t n);
extern "C" void* memcpy(void* __restrict__ dest, const void* __restrict__ src, size_t n);

namespace rpi {

constexpr char UART_RHR       = 0x00; // R
constexpr char UART_THR       = 0x00; // W
constexpr char UART_IER       = 0x01; // R/W
constexpr char UART_IIR       = 0x02; // R
constexpr char UART_FCR       = 0x02; // W
constexpr char UART_LCR       = 0x03; // R/W
constexpr char UART_MCR       = 0x04; // R/W
constexpr char UART_LSR       = 0x05; // R
constexpr char UART_MSR       = 0x06; // R
constexpr char UART_SPR       = 0x07; // R/W
constexpr char UART_TXLVL     = 0x08; // R
constexpr char UART_RXLVL     = 0x09; // R
constexpr char UART_IODir     = 0x0A; // R/W
constexpr char UART_IOState   = 0x0B; // R/W
constexpr char UART_IOIntEna  = 0x0C; // R/W
constexpr char UART_reserved  = 0x0D;
constexpr char UART_IOControl = 0x0E; // R/W
constexpr char UART_EFCR      = 0x0F; // R/W

constexpr char UART_DLL       = 0x00; // R/W - only accessible when EFR[4] = 1 and MCR[2] = 1
constexpr char UART_DLH       = 0x01; // R/W - only accessible when EFR[4] = 1 and MCR[2] = 1
constexpr char UART_EFR       = 0x02; // ?   - only accessible when LCR is 0xBF
constexpr char UART_TCR       = 0x06; // R/W - only accessible when EFR[4] = 1 and MCR[2] = 1
constexpr char UART_TLR       = 0x07; // R/W - only accessible when EFR[4] = 1 and MCR[2] = 1

} // namespace rpi

void uart_write_register(size_t spiChannel, size_t uartChannel, char reg, char data);
// "prepare" starts from index 1,
void uart_write_register(size_t spi_channel, size_t uart_channel, char reg, char *prepare, size_t len);
char uart_read_register(size_t spiChannel, size_t uartChannel, char reg);

uint32_t read_gpeds();
void set_gpeds(uint32_t gpeds);

bool is_clear_to_send(size_t spi_channel, size_t uart_channel);
bool is_uart_readable(size_t spi_channel, size_t uart_channel);
bool is_uart_writable(size_t spi_channel, size_t uart_channel);
void uart_write(size_t spi_channel, size_t uart_channel, char c);
char uart_read(size_t spi_channel, size_t uart_channel);

#define DEBUG_LITERAL(s) uart_puts(0, 0, s, LEN_LITERAL(s))
