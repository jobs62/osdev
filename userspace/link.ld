/*
*  link.ld
*/
OUTPUT_FORMAT(elf32-i386)
ENTRY(_start)

SECTIONS{
	.text BLOCK (4096) : {
        *(.text)
	}
	
	.rodata BLOCK (4096) : {
		*(.rodata)
	}
	
	.data BLOCK (4096) : {
		*(.data)
	}
	
	.bss BLOCK(4096) : {
		*(.bss)
	}
}
