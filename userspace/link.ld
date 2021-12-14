/*
*  link.ld
*/
OUTPUT_FORMAT(elf32-i386)
ENTRY(_start)

SECTIONS{
	.text ALIGN (4096) : {
        *(.text)
	}
	
	.rodata ALIGN (4096) : {
		*(.rodata)
	}
	
	.data ALIGN (4096) : {
		*(.data)
	}
	
	.bss : {
		*(.bss)
	}
}
