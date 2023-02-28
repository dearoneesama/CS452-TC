ENTRY(_start)           /* ELF entry symbol - no strictly needed */

STACKSIZE = DEFINED(STACKSIZE) ? STACKSIZE : 256M; /* kernel's stack size */

MEMORY {
    ram (rwx) : ORIGIN = 0x80000, LENGTH = 1024M /* Kernel load address for AArch64 */
}

SECTIONS {
    .text.boot : {      /* boot code must start at 0x80000 */
      KEEP(*(.text.boot))
    } > ram
    .bss (NOLOAD) : {
        . = ALIGN(16);
        __bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        __bss_end = .;
    } > ram
    .init_array : ALIGN(4) {  /* https://forums.raspberrypi.com/viewtopic.php?t=34925 */
      __init_array_start = .;
        *(.init_array)
        *(SORT_BY_INIT_PRIORITY(.init_array.*))
      __init_array_end = .;
    } > ram
    .stack (NOLOAD) : {  /* https://forums.raspberrypi.com/viewtopic.php?t=329527 */
        FILL(0xBAD0BAD0)
        . = ALIGN(16);
        __stack_limit = . ;
        *(.stack*)
        . = . + STACKSIZE;
        __stack_top = . ;
    } > ram
    _end = .;
}
__bss_size = (__bss_end - __bss_start)>>3;
