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

.global Exit
.balign 16
Exit:
    svc SYSCALLN_EXIT
    ret
