.global kernel_to_task_r
.balign 16
kernel_to_task_r:
    b kernel_to_task

.global kernel_to_task
.balign 16
kernel_to_task:
    // assume that x0 has pointer to kernel context
    // and that x1 has pointer to task context
    str x0, [sp, #-16]! // first store a pointer to the context of the kernel
    str x1, [sp, #-16]! // and also store the pointer to the context of the task

    // then, store the register file into the context of the kernel
    add x0, x0, #8
    stp x1, x2, [x0], #16
    stp x3, x4, [x0], #16
    stp x5, x6, [x0], #16
    stp x7, x8, [x0], #16
    stp x9, x10, [x0], #16
    stp x11, x12, [x0], #16
    stp x13, x14, [x0], #16
    stp x15, x16, [x0], #16
    stp x17, x18, [x0], #16
    stp x19, x20, [x0], #16
    stp x21, x22, [x0], #16
    stp x23, x24, [x0], #16
    stp x25, x26, [x0], #16
    stp x27, x28, [x0], #16
    stp x29, x30, [x0], #16

    add x1, x1, #248 // now x1 points to the spsr
    ldr x0, [x1, #0]
    msr SPSR_EL1, x0
    ldr x0, [x1, #8]
    msr SP_EL0, x0
    ldr x0, [x1, #16]
    msr ELR_EL1, x0

    // now we restore the task context
    ldr x0, [sp]

    add x0, x0, #8
    ldp x1, x2, [x0], #16
    ldp x3, x4, [x0], #16
    ldp x5, x6, [x0], #16
    ldp x7, x8, [x0], #16
    ldp x9, x10, [x0], #16
    ldp x11, x12, [x0], #16
    ldp x13, x14, [x0], #16
    ldp x15, x16, [x0], #16
    ldp x17, x18, [x0], #16
    ldp x19, x20, [x0], #16
    ldp x21, x22, [x0], #16
    ldp x23, x24, [x0], #16
    ldp x25, x26, [x0], #16
    ldp x27, x28, [x0], #16
    ldp x29, x30, [x0], #16

    // lastly restore x0
    ldr x0, [sp] // get the pointer
    ldr x0, [x0] // dereference it
    eret

.global task_to_kernel
.balign 16
task_to_kernel:
    // the pointer to the task context is on the stack
    // NOTE: do NOT use x0 or x1, because it contains data
    ldr x10, [sp], #16
    // store the register file into the context
    str x0, [x10]
    // now we can use x0
    add x0, x10, #8
    stp x1, x2, [x0], #16
    stp x3, x4, [x0], #16
    stp x5, x6, [x0], #16
    stp x7, x8, [x0], #16
    stp x9, x10, [x0], #16
    stp x11, x12, [x0], #16
    stp x13, x14, [x0], #16
    stp x15, x16, [x0], #16
    stp x17, x18, [x0], #16
    stp x19, x20, [x0], #16
    stp x21, x22, [x0], #16
    stp x23, x24, [x0], #16
    stp x25, x26, [x0], #16
    stp x27, x28, [x0], #16
    stp x29, x30, [x0], #16
    // general registers are stored, now store the special registers
    mrs x1, SPSR_EL1
    str x1, [x0], #8
    mrs x1, SP_EL0
    str x1, [x0], #8
    mrs x1, ELR_EL1
    str x1, [x0], #8
    // now restore kernel context
    ldr x0, [sp], #16
    add x0, x0, #8
    ldp x1, x2, [x0], #16
    ldp x3, x4, [x0], #16
    ldp x5, x6, [x0], #16
    ldp x7, x8, [x0], #16
    ldp x9, x10, [x0], #16
    ldp x11, x12, [x0], #16
    ldp x13, x14, [x0], #16
    ldp x15, x16, [x0], #16
    ldp x17, x18, [x0], #16
    ldp x19, x20, [x0], #16
    ldp x21, x22, [x0], #16
    ldp x23, x24, [x0], #16
    ldp x25, x26, [x0], #16
    ldp x27, x28, [x0], #16
    ldp x29, x30, [x0], #16

    mrs x0, ESR_EL1
    ret

.global initialize_kernel
.balign 16
initialize_kernel:
    ldr x0, =vector_exception_table
    MSR VBAR_EL1, x0
    ret

// Follows the sample vector exception table given by
// https://developer.arm.com/documentation/100933/0100/AArch64-exception-vector-table
.global vector_exception_table
.balign 0x800
vector_exception_table:
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

    b task_to_kernel // synchronous, lower EL 64

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
