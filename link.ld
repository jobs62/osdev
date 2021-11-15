/*
*  link.ld
*/
OUTPUT_FORMAT(elf32-i386)
ENTRY(start)

SECTIONS{
	. = 0x00100000;
	
	.text :{
		*(.multiboot)
        	*(.text)
	}
	
	.rodata ALIGN (4096) : {
		*(.rodata)
	}
	
	.data ALIGN (4096) : {
		*(.data)
	}
	
	.bss : {
		*(COMMON)
		*(.bss)
	}
}
