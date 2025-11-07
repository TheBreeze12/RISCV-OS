OUTPUT_ARCH( "riscv" )
ENTRY( main )

SECTIONS
{
  . = 0x0;
  
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