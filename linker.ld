ENTRY(_start)           /* ELF entry symbol - no strictly needed */

SECTIONS {
    . = 0x80000;        /* Kernel load address for AArch64 */
    .text.boot : {      /* boot code must start at 0x80000 */
      KEEP(*(.text.boot))
    }
    .bss (NOLOAD) : {
        . = ALIGN(16);
        __bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        __bss_end = .;
    }
    .init_array : ALIGN(4) {  /* https://forums.raspberrypi.com/viewtopic.php?t=34925 */
      __init_array_start = .;
        *(.init_array)
        *(SORT_BY_INIT_PRIORITY(.init_array.*))
      __init_array_end = .;
    }
    _end = .;
}
__bss_size = (__bss_end - __bss_start)>>3;
