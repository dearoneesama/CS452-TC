#include "rpi.hpp"
#include <queue>

void initialize() {
  // run static/global constructors manually
  using constructor_t = void (*)();
  extern constructor_t __init_array_start[], __init_array_end[];
  for (auto *fn = __init_array_start; fn < __init_array_end; (*fn++)());
}

namespace std {
  void __throw_length_error(char const*) { __builtin_unreachable(); }
}

template<class T>
struct dummy_allocator {
  using value_type = T;
  char *buffer;
  dummy_allocator(char *buffer_ = nullptr) : buffer{buffer_} {}
  [[nodiscard]] T *allocate(std::size_t) { return buffer; }
  void deallocate(T *, std::size_t) noexcept {}
};

template<class ...Ts>
struct pqueue : public std::priority_queue<Ts...> {
  using typename std::priority_queue<Ts...>::priority_queue;
  void reserve(std::size_t r) { this->c.reserve(r); }
};

int main() {
  initialize();
  init_gpio();
  init_spi(0);
  init_uart(0);

  char buf[200];
  auto x = pqueue<char, std::vector<char, dummy_allocator<char>>>(dummy_allocator<char>(buf));
  x.reserve(200);
  x.push('a');x.push('g');x.push('j');x.push('c');x.push('b');
  for (int i = 0; i < 5; ++i) {
    char c = x.top();
    x.pop();
    uart_putc(0, 0, c);
  }

  char msg1[] = "Hello world, this is iotest (" __TIME__ ")\r\nPress 'q' to reboot\r\n";
  uart_puts(0, 0, msg1, sizeof(msg1) - 1);
  char prompt[] = "PI> ";
  uart_puts(0, 0, prompt, sizeof(prompt) - 1);
  char c = ' ';
  while (c != 'q') {
    c = uart_getc(0, 0);
    if (c == '\r') {
      uart_puts(0, 0, "\r\n", 2);
      uart_puts(0, 0, prompt, sizeof(prompt) - 1);
    } else {
      uart_putc(0, 0, c);
    }
  }
  uart_puts(0, 0, "\r\n", 2);
  return 0;
}
