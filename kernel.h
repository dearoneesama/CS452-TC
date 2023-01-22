#pragma once
#include <stdint.h>

extern "C" int kernal_sys_call();

extern "C" int activate(int spsr, void (*elr)(), uint64_t* sp_el0);

extern "C" int* make_stack();

extern "C" void initialize_kernel();
