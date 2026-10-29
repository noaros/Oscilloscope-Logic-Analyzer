/* Linker script for the STM32F429ZI (Nucleo-F429ZI).
 * RM0090 section 2.3 (memory map) and 2.4 (embedded SRAM). */

ENTRY(Reset_Handler)

MEMORY
{
  FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 2048K
  RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 192K   /* SRAM1 + SRAM2 + SRAM3, contiguous */
  CCM   (rw)  : ORIGIN = 0x10000000, LENGTH = 64K    /* CPU only: DMA cannot reach it */
}

_estack = ORIGIN(RAM) + LENGTH(RAM);   /* initial stack pointer, stack grows down */
_min_stack_size = 0x1000;              /* 4 KB reserved below _estack; the heap may not grow into it */

SECTIONS
{
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector))
  } > FLASH

  .text :
  {
    . = ALIGN(4);
    *(.text*)
    *(.rodata*)
    KEEP(*(.init))
    KEEP(*(.fini))
    . = ALIGN(4);
  } > FLASH

  .ARM.extab : { *(.ARM.extab* .gnu.linkonce.armextab.*) } > FLASH
  .ARM.exidx :
  {
    __exidx_start = .;
    *(.ARM.exidx* .gnu.linkonce.armexidx.*)
    __exidx_end = .;
  } > FLASH

  .preinit_array :
  {
    PROVIDE_HIDDEN(__preinit_array_start = .);
    KEEP(*(.preinit_array*))
    PROVIDE_HIDDEN(__preinit_array_end = .);
  } > FLASH
  .init_array :
  {
    PROVIDE_HIDDEN(__init_array_start = .);
    KEEP(*(SORT(.init_array.*)))
    KEEP(*(.init_array*))
    PROVIDE_HIDDEN(__init_array_end = .);
  } > FLASH
  .fini_array :
  {
    PROVIDE_HIDDEN(__fini_array_start = .);
    KEEP(*(SORT(.fini_array.*)))
    KEEP(*(.fini_array*))
    PROVIDE_HIDDEN(__fini_array_end = .);
  } > FLASH

  /* Initialised data: lives in RAM, its initial values are stored in flash at _sidata. */
  _sidata = LOADADDR(.data);
  .data :
  {
    . = ALIGN(4);
    _sdata = .;
    *(.data*)
    . = ALIGN(4);
    _edata = .;
  } > RAM AT> FLASH

  .bss (NOLOAD) :
  {
    . = ALIGN(4);
    _sbss = .;
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    _ebss = .;
  } > RAM

  /* Uninitialised buffers placed with __attribute__((section(".ccmram"))). */
  .ccmram (NOLOAD) :
  {
    . = ALIGN(4);
    *(.ccmram*)
    . = ALIGN(4);
  } > CCM

  /* The heap (used by malloc / printf) starts after .bss and grows up towards the stack. */
  end = ALIGN(_ebss, 8);
  _heap_limit = _estack - _min_stack_size;
  ASSERT(end <= _heap_limit, "RAM overflow: .data + .bss leave no room for the stack")
}
