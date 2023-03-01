#pragma once
#include "kstddefs.hpp"

extern "C" int Create(priority_t priority, void (*function)());
extern "C" int MyTid();
extern "C" int MyParentTid();
extern "C" void Yield();
extern "C" void Exit();
extern "C" int AwaitEvent(int eventid);

// message passing
extern "C" int Send(int tid, const char* msg, int msglen, char* reply, int rplen);
extern "C" int Receive(int* tid, char* msg, int msglen);
extern "C" int Reply(int tid, const char* reply, int rplen);

// wrapper system calls
// names must be null-terminated
int RegisterAs(const char* name);
int WhoIs(const char* name);

// wrapper system calls for clock
int Time(int tid);
int Delay(int tid, int ticks);
int DelayUntil(int tid, int ticks);

// benchmarking
extern "C" void DCache();
extern "C" void ICache();
extern "C" void BCache();

// things to do while idling
extern "C" void TimeDistribution(time_distribution_t* time_distribution);

// put cpu into low power
extern "C" void SaveThePlanet();

// uart
int Getc(int tid, int channel);
int Putc(int tid, int channel, char c);
