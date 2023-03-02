#include "user_syscall.include"

.global Create
.balign 16
Create:
    svc SYSCALLN_CREATE
    ret

.global MyTid
.balign 16
MyTid:
    svc SYSCALLN_MYTID
    ret

.global MyParentTid
.balign 16
MyParentTid:
    svc SYSCALLN_MYPARENTTID
    ret

.global Yield
.balign 16
Yield:
    svc SYSCALLN_YIELD
    ret

.global Send
.balign 16
Send:
    svc SYSCALLN_SEND
    ret

.global Receive
.balign 16
Receive:
    svc SYSCALLN_RECEIVE
    ret

.global Reply
.balign 16
Reply:
    svc SYSCALLN_REPLY
    ret

.global AwaitEvent
.balign 16
AwaitEvent:
    svc SYSCALLN_AWAITEVENT
    ret

.global Exit
.balign 16
Exit:
    svc SYSCALLN_EXIT
    ret

.global DCache
.balign 16
DCache:
    svc SYSBENCHMARK_DCACHE
    ret

.global ICache
.balign 16
ICache:
    svc SYSBENCHMARK_ICACHE
    ret

.global BCache
.balign 16
BCache:
    svc SYSBENCHMARK_BCACHE
    ret

.global SaveThePlanet
.balign 16
SaveThePlanet:
    svc SYSCALLN_SAVETHEPLANET
    ret

.global TimeDistribution
.balign 16
TimeDistribution:
    svc SYSCALLN_TIMEDISTRIBUTION
    ret

.global UartWriteRegister
.balign 16
UartWriteRegister:
    svc SYSCALLN_UARTWRITE
    ret

.global UartReadRegister
.balign 16
UartReadRegister:
    svc SYSCALLN_UARTREAD
    ret

