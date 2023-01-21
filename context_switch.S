.global activate
activate:
    // suppose x0: task spsr
    //         x1: ELR_EL1
    //         x2: SP_EL0
    b save_context

.global save_context
.balign 16
save_context:
    str x1, [sp, #-16]!
    str x2, [sp, #-16]!
    str x3, [sp, #-16]!
    str x4, [sp, #-16]!
    str x5, [sp, #-16]!
    str x6, [sp, #-16]!
    str x7, [sp, #-16]!
    str x8, [sp, #-16]!
    str x9, [sp, #-16]!
    str x10, [sp, #-16]!
    str x11, [sp, #-16]!
    str x12, [sp, #-16]!
    str x13, [sp, #-16]!
    str x14, [sp, #-16]!
    str x15, [sp, #-16]!
    str x16, [sp, #-16]!
    str x17, [sp, #-16]!
    str x18, [sp, #-16]!
    str x19, [sp, #-16]!
    str x20, [sp, #-16]!
    str x21, [sp, #-16]!
    str x22, [sp, #-16]!
    str x23, [sp, #-16]!
    str x24, [sp, #-16]!
    str x25, [sp, #-16]!
    str x26, [sp, #-16]!
    str x27, [sp, #-16]!
    str x28, [sp, #-16]!
    str x29, [sp, #-16]!
    str x30, [sp, #-16]!
    msr SPSR_EL1, x0
    msr ELR_EL1, x1
    msr SP_EL0, x2
    mov x0, #452
    eret

.global load_context
.balign 16
load_context:
    ldr x30, [sp], #16
    ldr x29, [sp], #16
    ldr x28, [sp], #16
    ldr x27, [sp], #16
    ldr x26, [sp], #16
    ldr x25, [sp], #16
    ldr x24, [sp], #16
    ldr x23, [sp], #16
    ldr x22, [sp], #16
    ldr x21, [sp], #16
    ldr x20, [sp], #16
    ldr x19, [sp], #16
    ldr x18, [sp], #16
    ldr x17, [sp], #16
    ldr x16, [sp], #16
    ldr x15, [sp], #16
    ldr x14, [sp], #16
    ldr x13, [sp], #16
    ldr x12, [sp], #16
    ldr x11, [sp], #16
    ldr x10, [sp], #16
    ldr x9, [sp], #16
    ldr x8, [sp], #16
    ldr x7, [sp], #16
    ldr x6, [sp], #16
    ldr x5, [sp], #16
    ldr x4, [sp], #16
    ldr x3, [sp], #16
    ldr x2, [sp], #16
    ldr x1, [sp], #16
    MRS x0, ESR_EL1
    ret

.global initialize
.balign 16
initialize:
    str x0, [sp, #-16]!
    ldr x0, =vector_table_tabl_el1
    MSR VBAR_EL1, x0
    ldr x0, [sp], #16
    ret

.global vector_table_tabl_el1
.balign 0x800
vector_table_tabl_el1:
    add x0, x0, #0 // synchronous, current EL SP0
    .balign 0x80
    add x0, x0, #0 // IRQ, current EL SP0
    .balign 0x80
    add x0, x0, #0 // FIQ, current EL SP0
    .balign 0x80
    add x0, x0, #0 // SError, current EL SP0
    .balign 0x80

    add x0, x0, #0 // synchronous, current EL SPX
    .balign 0x80
    add x0, x0, #0 // IRQ, current EL SPX
    .balign 0x80
    add x0, x0, #0 // FIQ, current EL SPX
    .balign 0x80
    add x0, x0, #0 // SError, current EL SPX
    .balign 0x80

    b load_context // synchronous, lower EL 64

    .balign 0x80
    add x0, x0, #0 // IRQ, lower EL 64
    .balign 0x80
    add x0, x0, #0 // FIQ, lower EL 64
    .balign 0x80
    add x0, x0, #0 // SError, lower EL 64
    .balign 0x80

    add x0, x0, #0 // synchronous, lower EL 32
    .balign 0x80
    add x0, x0, #0 // IRQ, lower EL 32
    .balign 0x80
    add x0, x0, #0 // FIQ, lower EL 32
    .balign 0x80
    add x0, x0, #0 // SError, lower EL 32
    .balign 0x80

.global my_sys_call
.balign 16
my_sys_call:
    svc 1
    ret

.global make_stack
.balign 16
make_stack:
    add sp, sp, #512
    mov x0, sp
    ret
