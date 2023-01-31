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

    str x1, [x0, #8]! // then, store the register file into the context of the kernel
    str x2, [x0, #8]!
    str x3, [x0, #8]!
    str x4, [x0, #8]!
    str x5, [x0, #8]!
    str x6, [x0, #8]!
    str x7, [x0, #8]!
    str x8, [x0, #8]!
    str x9, [x0, #8]!
    str x10, [x0, #8]!
    str x11, [x0, #8]!
    str x12, [x0, #8]!
    str x13, [x0, #8]!
    str x14, [x0, #8]!
    str x15, [x0, #8]!
    str x16, [x0, #8]!
    str x17, [x0, #8]!
    str x18, [x0, #8]!
    str x19, [x0, #8]!
    str x20, [x0, #8]!
    str x21, [x0, #8]!
    str x22, [x0, #8]!
    str x23, [x0, #8]!
    str x24, [x0, #8]!
    str x25, [x0, #8]!
    str x26, [x0, #8]!
    str x27, [x0, #8]!
    str x28, [x0, #8]!
    str x29, [x0, #8]!
    str x30, [x0, #8]!

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
    ldr x1, [x0], #8
    ldr x2, [x0], #8
    ldr x3, [x0], #8
    ldr x4, [x0], #8
    ldr x5, [x0], #8
    ldr x6, [x0], #8
    ldr x7, [x0], #8
    ldr x8, [x0], #8
    ldr x9, [x0], #8
    ldr x10, [x0], #8
    ldr x11, [x0], #8
    ldr x12, [x0], #8
    ldr x13, [x0], #8
    ldr x14, [x0], #8
    ldr x15, [x0], #8
    ldr x16, [x0], #8
    ldr x17, [x0], #8
    ldr x18, [x0], #8
    ldr x19, [x0], #8
    ldr x20, [x0], #8
    ldr x21, [x0], #8
    ldr x22, [x0], #8
    ldr x23, [x0], #8
    ldr x24, [x0], #8
    ldr x25, [x0], #8
    ldr x26, [x0], #8
    ldr x27, [x0], #8
    ldr x28, [x0], #8
    ldr x29, [x0], #8
    ldr x30, [x0], #8

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
    mov x0, x10
    str x1, [x0, #8]!
    str x2, [x0, #8]!
    str x3, [x0, #8]!
    str x4, [x0, #8]!
    str x5, [x0, #8]!
    str x6, [x0, #8]!
    str x7, [x0, #8]!
    str x8, [x0, #8]!
    str x9, [x0, #8]!
    str x10, [x0, #8]!
    str x11, [x0, #8]!
    str x12, [x0, #8]!
    str x13, [x0, #8]!
    str x14, [x0, #8]!
    str x15, [x0, #8]!
    str x16, [x0, #8]!
    str x17, [x0, #8]!
    str x18, [x0, #8]!
    str x19, [x0, #8]!
    str x20, [x0, #8]!
    str x21, [x0, #8]!
    str x22, [x0, #8]!
    str x23, [x0, #8]!
    str x24, [x0, #8]!
    str x25, [x0, #8]!
    str x26, [x0, #8]!
    str x27, [x0, #8]!
    str x28, [x0, #8]!
    str x29, [x0, #8]!
    str x30, [x0, #8]!
    // general registers are stored, now store the special registers
    mrs x1, SPSR_EL1
    str x1, [x0, #8]!
    mrs x1, SP_EL0
    str x1, [x0, #8]!
    mrs x1, ELR_EL1
    str x1, [x0, #8]!
    // now restore kernel context
    ldr x0, [sp], #16
    add x0, x0, #8
    ldr x1, [x0], #8
    ldr x2, [x0], #8
    ldr x3, [x0], #8
    ldr x4, [x0], #8
    ldr x5, [x0], #8
    ldr x6, [x0], #8
    ldr x7, [x0], #8
    ldr x8, [x0], #8
    ldr x9, [x0], #8
    ldr x10, [x0], #8
    ldr x11, [x0], #8
    ldr x12, [x0], #8
    ldr x13, [x0], #8
    ldr x14, [x0], #8
    ldr x15, [x0], #8
    ldr x16, [x0], #8
    ldr x17, [x0], #8
    ldr x18, [x0], #8
    ldr x19, [x0], #8
    ldr x20, [x0], #8
    ldr x21, [x0], #8
    ldr x22, [x0], #8
    ldr x23, [x0], #8
    ldr x24, [x0], #8
    ldr x25, [x0], #8
    ldr x26, [x0], #8
    ldr x27, [x0], #8
    ldr x28, [x0], #8
    ldr x29, [x0], #8
    ldr x30, [x0], #8

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
