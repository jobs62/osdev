/*
*  link.ld
*/
OUTPUT_FORMAT(elf32-i386)
ENTRY(start)

SECTIONS{
	. = 0xC0100000;
	
	.text : AT(ADDR(.text) - 0xC0000000) {
		*(.multiboot)
        *(.text)
	}
	
	.rodata ALIGN (4096) : AT(ADDR(.rodata) - 0xC0000000) {
		*(.rodata)
	}
	
	.data ALIGN (4096) : AT(ADDR(.data) - 0xC0000000) {
		*(.data)
	}
	
	.bss : AT(ADDR(.bss) - 0xC0000000) {
		*(COMMON)
		*(.bss)
	}
}
