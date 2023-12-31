#include "rpi.hpp"

struct GPIO {
  uint32_t GPFSEL[6];
  uint32_t : 32;
  uint32_t GPSET0;
  uint32_t GPSET1;
  uint32_t : 32;
  uint32_t GPCLR0;
  uint32_t GPCLR1;
  uint32_t : 32;
  uint32_t GPLEV0;
  uint32_t GPLEV1;
  uint32_t : 32;
  uint32_t GPEDS0;
  uint32_t GPEDS1;
  uint32_t : 32;
  uint32_t GPREN0;
  uint32_t GPREN1;
  uint32_t : 32;
  uint32_t GPFEN0;
  uint32_t GPFEN1;
  uint32_t : 32;
  uint32_t GPHEN0;
  uint32_t GPHEN1;
  uint32_t : 32;
  uint32_t GPLEN0;
  uint32_t GPLEN1;
  uint32_t : 32;
  uint32_t GPAREN0;
  uint32_t GPAREN1;
  uint32_t : 32;
  uint32_t GPAFEN0;
  uint32_t GPAFEN1;
  uint32_t _unused[21];
  uint32_t PUP_PDN_CNTRL_REG[4];
};

struct AUX {
  uint32_t IRQ;
  uint32_t ENABLES;
};

struct SPI {
  uint32_t CNTL0;       // Control register 0
  uint32_t CNTL1;       // Control register 1
  uint32_t STAT;        // Status
  uint32_t PEEK;        // Peek
  uint32_t _unused[4];
  uint32_t IO_REGa;     // Data
  uint32_t IO_REGb;     // Data
  uint32_t IO_REGc;     // Data
  uint32_t IO_REGd;     // Data
  uint32_t TXHOLD_REGa; // Extended Data
  uint32_t TXHOLD_REGb; // Extended Data
  uint32_t TXHOLD_REGc; // Extended Data
  uint32_t TXHOLD_REGd; // Extended Data
};

static char* const MMIO_BASE = (char*)           0xFE000000;
static char* const GPIO_BASE = (char*)(MMIO_BASE + 0x200000);
static char* const  AUX_BASE = (char*)(GPIO_BASE +  0x15000);

static volatile struct GPIO* const gpio  =  (struct GPIO*)(GPIO_BASE);
static volatile struct AUX*  const aux   =   (struct AUX*)(AUX_BASE);
static volatile struct SPI*  const spi[] = { (struct SPI*)(AUX_BASE + 0x80), (struct SPI*)(AUX_BASE + 0xC0) };

uint32_t read_gpeds() {
  return gpio->GPEDS0;
}

void set_gpeds(uint32_t gpeds) {
  gpio->GPEDS0 = gpeds;
}

/*************** GPIO ***************/

static const uint32_t GPIO_INPUT  = 0x00;
static const uint32_t GPIO_OUTPUT = 0x01;
static const uint32_t GPIO_ALTFN0 = 0x04;
static const uint32_t GPIO_ALTFN1 = 0x05;
static const uint32_t GPIO_ALTFN2 = 0x06;
static const uint32_t GPIO_ALTFN3 = 0x07;
static const uint32_t GPIO_ALTFN4 = 0x03;
static const uint32_t GPIO_ALTFN5 = 0x02;

static const uint32_t GPIO_NONE = 0x00;
static const uint32_t GPIO_PUP  = 0x01;
static const uint32_t GPIO_PDP  = 0x02;

static void setup_gpio(uint32_t pin, uint32_t setting, uint32_t resistor) {
  uint32_t reg   =  pin / 10;
  uint32_t shift = (pin % 10) * 3;
  uint32_t status = gpio->GPFSEL[reg];   // read status
  status &= ~(7u << shift);              // clear bits
  status |=  (setting << shift);         // set bits
  gpio->GPFSEL[reg] = status;            // write back

  reg   =  pin / 16;
  shift = (pin % 16) * 2;
  status = gpio->PUP_PDN_CNTRL_REG[reg]; // read status
  status &= ~(3u << shift);              // clear bits
  status |=  (resistor << shift);        // set bits
  gpio->PUP_PDN_CNTRL_REG[reg] = status; // write back
}

void init_gpio() {
  setup_gpio(18, GPIO_ALTFN4, GPIO_NONE);
  setup_gpio(19, GPIO_ALTFN4, GPIO_NONE);
  setup_gpio(20, GPIO_ALTFN4, GPIO_NONE);
  setup_gpio(21, GPIO_ALTFN4, GPIO_NONE);

  setup_gpio(24, GPIO_INPUT, GPIO_NONE);
  gpio->GPLEN0 |= (1 << 24);
}

static const uint32_t SPI_CNTL0_DOUT_HOLD_SHIFT = 12;
static const uint32_t SPI_CNTL0_CS_SHIFT        = 17;
static const uint32_t SPI_CNTL0_SPEED_SHIFT = 20;

static const uint32_t SPI_CNTL0_POSTINPUT   = 0x00010000;
static const uint32_t SPI_CNTL0_VAR_CS      = 0x00008000;
static const uint32_t SPI_CNTL0_VAR_WIDTH   = 0x00004000;
static const uint32_t SPI_CNTL0_Enable      = 0x00000800;
static const uint32_t SPI_CNTL0_In_Rising   = 0x00000400;
static const uint32_t SPI_CNTL0_Clear_FIFOs = 0x00000200;
static const uint32_t SPI_CNTL0_Out_Rising  = 0x00000100;
static const uint32_t SPI_CNTL0_Invert_CLK  = 0x00000080;
static const uint32_t SPI_CNTL0_SO_MSB_FST  = 0x00000040;
static const uint32_t SPI_CNTL0_MAX_SHIFT   = 0x0000003F;

static const uint32_t SPI_CNTL1_CS_HIGH_SHIFT   = 8;

static const uint32_t SPI_CNTL1_Keep_Input  = 0x00000001;
static const uint32_t SPI_CNTL1_SI_MSB_FST  = 0x00000002;
static const uint32_t SPI_CNTL1_Done_IRQ    = 0x00000040;
static const uint32_t SPI_CNTL1_TX_EM_IRQ   = 0x00000080;

static const uint32_t SPI_STAT_TX_FIFO_MASK = 0xFF000000;
static const uint32_t SPI_STAT_RX_FIFO_MASK = 0x00FF0000;
static const uint32_t SPI_STAT_TX_FULL      = 0x00000400;
static const uint32_t SPI_STAT_TX_EMPTY     = 0x00000200;
static const uint32_t SPI_STAT_RX_FULL      = 0x00000100;
static const uint32_t SPI_STAT_RX_EMPTY     = 0x00000080;
static const uint32_t SPI_STAT_BUSY         = 0x00000040;
static const uint32_t SPI_STAT_BIT_CNT_MASK = 0x0000003F;

void init_spi(uint32_t channel) {
  uint32_t reg = aux->ENABLES;
  reg |= (2 << channel);
  aux->ENABLES = reg;
  spi[channel]->CNTL0 = SPI_CNTL0_Clear_FIFOs;
  uint32_t speed = (700000000 / (2 * 0x400000)) - 1; // for maximum bitrate 0x400000
  spi[channel]->CNTL0 = (speed << SPI_CNTL0_SPEED_SHIFT)
                      | SPI_CNTL0_VAR_WIDTH
                      | SPI_CNTL0_Enable
                      | SPI_CNTL0_In_Rising
                      | SPI_CNTL0_SO_MSB_FST;
  spi[channel]->CNTL1 = SPI_CNTL1_SI_MSB_FST;
}

static void spi_send_recv(uint32_t channel, const char* sendbuf, size_t sendlen, char* recvbuf, size_t recvlen) {
  size_t sendidx = 0;
  size_t recvidx = 0;
  while (sendidx < sendlen || recvidx < recvlen) {
    uint32_t data = 0;
    size_t count = 0;

    // prepare write data
    for (; sendidx < sendlen && count < 24; sendidx += 1, count += 8) {
      data |= (sendbuf[sendidx] << (16 - count));
    }
    data |= (count << 24);

    // always need to write something, otherwise no receive
    while (spi[channel]->STAT & SPI_STAT_TX_FULL) asm volatile("yield");
    if (sendidx < sendlen) {
      spi[channel]->TXHOLD_REGa = data; // keep chip-select active, more to come
    } else {
      spi[channel]->IO_REGa = data;
    }

    // read transaction
    while (spi[channel]->STAT & SPI_STAT_RX_EMPTY) asm volatile("yield");
    data = spi[channel]->IO_REGa;

    // process data, if needed, assume same byte count in transaction
    size_t max = (recvlen - recvidx) * 8;
    if (count > max) count = max;
    for (; count > 0; recvidx += 1, count -= 8) {
      recvbuf[recvidx] = (data >> (count - 8)) & 0xFF;
    }
  }
}

/*************** SPI ***************/

using namespace rpi;

// UART flags
static const char UART_CHANNEL_SHIFT           = 1;
static const char UART_ADDR_SHIFT              = 3;
static const char UART_READ_ENABLE             = 0x80;
static const char UART_FCR_TX_FIFO_RESET       = 0x04;
static const char UART_FCR_RX_FIFO_RESET       = 0x02;
static const char UART_FCR_FIFO_EN             = 0x01;
static const char UART_LCR_DIV_LATCH_EN        = 0x80;
static const char UART_EFR_ENABLE_ENHANCED_FNS = 0x10;
static const char UART_AUTO_CTS                = (1 << 7);
static const char UART_IOControl_RESET         = 0x08;

void uart_write_register(size_t spiChannel, size_t uartChannel, char reg, char data) {
  char req[2] = {0};
  req[0] = (uartChannel << UART_CHANNEL_SHIFT) | (reg << UART_ADDR_SHIFT);
  req[1] = data;
  spi_send_recv(spiChannel, req, 2, NULL, 0);
}

void uart_write_register(size_t spi_channel, size_t uart_channel, char reg, char *prepare, size_t len) {
  prepare[0] = (uart_channel << UART_CHANNEL_SHIFT) | (reg << UART_ADDR_SHIFT);
  spi_send_recv(spi_channel, prepare, len + 1, nullptr, 0);
}

char uart_read_register(size_t spiChannel, size_t uartChannel, char reg) {
  char req[2] = {0};
  char res[2] = {0};
  req[0] = (uartChannel << UART_CHANNEL_SHIFT) | (reg << UART_ADDR_SHIFT) | UART_READ_ENABLE;
  spi_send_recv(spiChannel, req, 2, res, 2);
  return res[1];
}

static void uart_init_channel(size_t spiChannel, size_t uartChannel, size_t baudRate) {
  // set baud rate
  uart_write_register(spiChannel, uartChannel, UART_LCR, UART_LCR_DIV_LATCH_EN);
  uint32_t bauddiv = 14745600 / (baudRate * 16);
  uart_write_register(spiChannel, uartChannel, UART_DLH, (bauddiv & 0xFF00) >> 8);
  uart_write_register(spiChannel, uartChannel, UART_DLL, (bauddiv & 0x00FF));

  bool is_channel_0 = uartChannel == 0;
  uart_write_register(spiChannel, uartChannel, UART_LCR, 0xbf);
  if (is_channel_0) {
    uart_write_register(spiChannel, uartChannel, UART_EFR, UART_EFR_ENABLE_ENHANCED_FNS);
  } else {
    // TODO: investigate
    // uart_write_register(spiChannel, uartChannel, UART_EFR, UART_AUTO_CTS);
  }

  char lcr_data = is_channel_0 ? 0x3 : 0x7;
  // set serial byte configuration: 8 bit, no parity, 1 stop bit
  uart_write_register(spiChannel, uartChannel, UART_LCR, lcr_data);

  char fcr_data = UART_FCR_RX_FIFO_RESET | UART_FCR_TX_FIFO_RESET;
  if (is_channel_0) {
    // uart_write_register(spiChannel, uartChannel, UART_EFR, UART_EFR_ENABLE_ENHANCED_FNS);
    fcr_data |= UART_FCR_FIFO_EN;
  }
  // clear and enable fifos, (wait since clearing fifos takes time)
  uart_write_register(spiChannel, uartChannel, UART_FCR, fcr_data);
  for (int i = 0; i < 65535; ++i) asm volatile("yield");
}

void init_uart(uint32_t spiChannel) {
  uart_write_register(spiChannel, 0, UART_IOControl, UART_IOControl_RESET); // resets both channels
  uart_init_channel(spiChannel, 0, 115200);
  uart_init_channel(spiChannel, 1,   2400);
}

bool is_clear_to_send(size_t spi_channel, size_t uart_channel) {
  return (uart_read_register(spi_channel, uart_channel, UART_MSR) & (1 << 4)) != 0;
}

bool is_uart_readable(size_t spi_channel, size_t uart_channel) {
  return uart_read_register(spi_channel, uart_channel, UART_RXLVL) != 0;
}

bool is_uart_writable(size_t spi_channel, size_t uart_channel) {
  return uart_read_register(spi_channel, uart_channel, UART_TXLVL) != 0;
}

void uart_write(size_t spi_channel, size_t uart_channel, char c) {
  uart_write_register(spi_channel, uart_channel, UART_THR, c);
}

char uart_read(size_t spi_channel, size_t uart_channel) {
  return uart_read_register(spi_channel, uart_channel, UART_RHR);
}

char uart_getc(size_t spiChannel, size_t uartChannel) {
  while (uart_read_register(spiChannel, uartChannel, UART_RXLVL) == 0) asm volatile("yield");
  return uart_read_register(spiChannel, uartChannel, UART_RHR);
}

void uart_putc(size_t spiChannel, size_t uartChannel, char c) {
  while (uart_read_register(spiChannel, uartChannel, UART_TXLVL) == 0) asm volatile("yield");
  uart_write_register(spiChannel, uartChannel, UART_THR, c);
}

void uart_puts(size_t spiChannel, size_t uartChannel, const char* buf, size_t blen) {
  static const size_t max = 32;
  char temp[max];
  temp[0] = (uartChannel << UART_CHANNEL_SHIFT) | (UART_THR << UART_ADDR_SHIFT);
  size_t tlen = uart_read_register(spiChannel, uartChannel, UART_TXLVL);
  if (tlen > max) tlen = max;
  for (size_t bidx = 0, tidx = 1;;) {
    if (tidx < tlen && bidx < blen) {
      temp[tidx] = buf[bidx];
      bidx += 1;
      tidx += 1;
    } else {
      spi_send_recv(spiChannel, temp, tidx, NULL, 0);
      if (bidx == blen) break;
      tlen = uart_read_register(spiChannel, uartChannel, UART_TXLVL);
      if (tlen > max) tlen = max;
      tidx = 1;
    }
  }
}

void uart_putui64(size_t spiChannel, size_t uartChannel, uint64_t n) {
  if (n == 0) {
    uart_putc(spiChannel, uartChannel, '0');
    return;
  }
  char digits[64];
  int len = 0;
  while (n != 0) {
    digits[len++] = n % 10;
    n /= 10;
  }
  for (--len; len >= 0; --len) {
    uart_putc(spiChannel, uartChannel, '0' + digits[len]);
  }
}

// define our own memset to avoid SIMD instructions emitted from the compiler
extern "C" void *memset(void *s, int c, size_t n) {
  for (char* it = (char*)s; n > 0; --n) *it++ = c;
  return s;
}

// define our own memcpy to avoid SIMD instructions emitted from the compiler
extern "C" void* memcpy(void* __restrict__ dest, const void* __restrict__ src, size_t n) {
  char* cdest = (char*)dest;
  char* csrc = (char*)src;
  uint64_t* u64dest = (uint64_t*)dest;
  uint64_t* u64src = (uint64_t*)src;

  // do not do block copy in these conditions
  auto dest_mod = (uintptr_t)dest & 7;
  auto src_mod = (uintptr_t)src & 7;
  if (
    n < 8  // too small
    || ((dest_mod || src_mod) && dest_mod != src_mod) // never aligns
  ) {
    while (n--) *(cdest++) = *(csrc++);
    return dest;
  }

  // try to align
  if (dest_mod) {
    auto remain = 8 - dest_mod;
    while (remain && n) {
      *(cdest++) = *(csrc++);
      --n;
      --remain;
    }
    if (!n) return dest;
  }

  u64dest = (uint64_t*)cdest;
  u64src = (uint64_t*)csrc;
  // this is because ARMv8 registers are 64bit or 8bytes
  while (n >= 8) {
    *(u64dest++) = *(u64src++);
    n -= 8;
  }

  cdest = (char*)u64dest;
  csrc = (char*)u64src;
  while (n--) *(cdest++) = *(csrc++);
  return dest;
}
