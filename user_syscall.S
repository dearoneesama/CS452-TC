.global Create
.balign 16
Create:
    svc 1
    ret

.global MyTid
.balign 16
MyTid:
    svc 2
    ret

.global MyParentTid
.balign 16
MyParentTid:
    svc 3
    ret

.global Yield
.balign 16
Yield:
    svc 4
    ret

.global Exit
.balign 16
Exit:
    svc 5
    ret
