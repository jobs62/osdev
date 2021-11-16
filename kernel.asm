;;kernel.asm

;nasm directive - 32 bit
bits 32
section .multiboot
        ;multiboot spec
        align 4
        dd 0x1BADB002            ;magic
        dd 0x00                  ;flags
        dd - (0x1BADB002 + 0x00) ;checksum. m+f+c should be zero

global start
extern kmain	        ;kmain is defined in the c file

start:
    cli 			;block interrupts
    mov esp, stack_space	;set stack pointer
    push ebx
    push eax

    call kmain
loop:
    ;hlt		 	;halt the CPU
    jmp loop

global load_gdt
extern gdt_base

load_gdt:
	lgdt [gdt_base]

	; enable protected mode
	mov eax, cr0
	or al, 1
	mov cr0, eax

	jmp 0x08:.load_gdt_cs_jmp2 ; Far Jump to load code segment
.load_gdt_cs_jmp2:
	
	mov ax, 0x10 ; Load data segmement selectors.
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	mov ss, ax
	ret


section .bss
    align 16
    resb 8192		;8KB for stack
stack_space:
    resb 4096
table_768:
