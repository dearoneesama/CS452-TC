#pragma once
#include "kstddefs.hpp"

extern "C" int Create(priority_t priority, void (*function)());
extern "C" int MyTid();
extern "C" int MyParentTid();
extern "C" void Yield();
extern "C" void Exit();

// message passing
extern "C" int Send(int tid, const char* msg, int msglen, char* reply, int rplen);
extern "C" int Receive(int* tid, char* msg, int msglen);
extern "C" int Reply(int tid, const char* reply, int rplen);

// wrapper system calls
// names must be null-terminated
int RegisterAs(const char* name);
int WhoIs(const char* name);
