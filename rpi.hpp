#pragma once

#include <stddef.h>
#include <stdint.h>

void init_gpio();
void init_spi(uint32_t channel);
void init_uart(uint32_t spiChannel);
char uart_getc(size_t spiChannel, size_t uartChannel);
void uart_putc(size_t spiChannel, size_t uartChannel, char c);
void uart_puts(size_t spiChannel, size_t uartChannel, const char* buf, size_t blen);
void uart_putui64(size_t spiChannel, size_t uartChannel, uint64_t n);
