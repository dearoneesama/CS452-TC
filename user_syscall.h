#pragma once
#include "kstddefs.hpp"

extern "C" int Create(priority_t priority, void (*function)());
extern "C" int MyTid();
extern "C" int MyParentTid();
extern "C" void Yield();
extern "C" void Exit();
