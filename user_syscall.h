#pragma once
#include "priority.hpp"

// svc 1
extern "C" int Create(priority_t priority, void (*function)());

// svc 2
extern "C" int MyTid();

// svc 3
extern "C" int MyParentTid();

// svc 4
extern "C" void Yield();

// svc 5
extern "C" void Exit();
