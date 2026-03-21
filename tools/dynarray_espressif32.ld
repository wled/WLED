/* ESP32 linker script fragment to add dynamic array section to binary */
SECTIONS
{
  .dynarray :
  {
    . = ALIGN(0x10);
    KEEP(*(SORT_BY_INIT_PRIORITY(.dynarray.*)))
  } > default_rodata_seg
}
INSERT AFTER .flash.rodata;
