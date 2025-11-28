/* Linker script fragment to add dynamic array section to binary */
SECTIONS
{
  .dynarray :
  {
    . = ALIGN(4);
    KEEP(*(SORT_BY_INIT_PRIORITY(.dynarray.*)))
  } > default_rodata_seg
}
