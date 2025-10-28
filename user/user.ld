OUTPUT_ARCH( "riscv" )
ENTRY( main )

SECTIONS
{
  . = 0x0;
  .text : {
    *(.text)
  }
  .data : {
    *(.data)
  }
  .bss : {
    *(.bss)
  }
}