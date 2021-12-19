/*
*  link.ld
*/
OUTPUT_FORMAT(elf32-i386)
ENTRY(_start)

SECTIONS{
	.text  : {
        *(.text)
	}
	
	.rodata : {
		*(.rodata)
	}
	
	.data : {
		*(.data)
	}
	
	.bss : {
		*(.bss)
	}
}
