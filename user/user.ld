OUTPUT_ARCH( "riscv" )
ENTRY( main )

SECTIONS
{
  . = 0x1000;  /* 改为 0x1000，避免地址 0 的问题 */
  
  .text : {
    *(.text .text.*)
    . = ALIGN(0x1000);
  }
  
  .rodata : {
    *(.rodata .rodata.*)
  }
  
  .data : {
    *(.data .data.*)
  }
  
  .bss : {
    *(.bss .bss.*)
    *(COMMON)
  }
  
  /DISCARD/ : {
    *(.eh_frame .note.GNU-stack)
  }
}