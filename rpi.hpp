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
extern "C" void *memset(void *s, int c, size_t n);
extern "C" void* memcpy(void* __restrict__ dest, const void* __restrict__ src, size_t n);

#define DEBUG_LITERAL(s) uart_puts(0, 0, s, LEN_LITERAL(s))
