#pragma once

int kernal_sys_call();

int activate(int spsr, void (*elr)(), int* sp_el0);

int* make_stack();

int initialize(int i);
