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
extern __kernel_ro_start
extern __kernel_ro_rw
extern __kernel_rw_end

start:
    cli 			;block interrupts

	; ok let's try to gen page table
	mov esp, (PAGE_TABLE_END - 0xC0000000)
	mov ecx, 0x00400000 - 0x1000 ;set eax to the end of the page
.table_page_loop_1: ; push zeros until identiy mapping for kernel rw
	push 0x00000000
	sub ecx, 0x00001000
	cmp ecx, (__kernel_rw_end - 0xC0000000)
	jge .table_page_loop_1 
.table_page_loop_2:
	mov edx, ecx
	and edx, 0xfffff000
	or edx,  0x00000003 ; read-write and present
	push edx
	sub ecx, 0x00001000
	cmp ecx, (__kernel_ro_rw - 0xC0000000)
	jge .table_page_loop_2
.table_page_loop_3:
	mov edx, ecx
	and edx, 0xfffff000
	or edx,  0x00000001 ; read-only and present
	push edx
	sub ecx, 0x00001000
	cmp ecx, (__kernel_ro_start - 0xC0000000)
	jge .table_page_loop_3
.table_page_loop_4: ; push zeros until then end
	push 0x00000000
	sub ecx, 0x00001000
	jge .table_page_loop_4
	mov DWORD [PAGE_TABLE - 0xc0000000 + 0xB8 * 4], (0x000b8003) ; map VGA video memory
	mov DWORD [PAGE_TABLE - 0xc0000000 + 0x9 * 4], (0x00009001) ; map mbi memory

    ; update page directory address, since eax and ebx is in use, have to use ecx or other register
    mov ecx, (PAGE_DIRECTORY - 0xC0000000)
    mov cr3, ecx

    ; Enable paging
    mov ecx, cr0
    or ecx, 0x80000000
    mov cr0, ecx

    ; Do a absolute jump to go to higher_half
    lea ecx, [higher_half]
    jmp ecx
higher_half:

    mov esp, stack_space	;set stack pointer
    push ebx
    push eax

    call kmain

loop:
    hlt		 	;halt the CPU
    jmp loop	;this should be non reachable code anyway

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


global tss_flush
tss_flush:
	mov ax, 0x2b
	ltr ax
	ret


global switch_to_user_mode
global stack_space
switch_to_user_mode:
	cli ;critical code section
	
	mov ax, 0x23 ; Load user data segmement selectors.
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	;mov ss, ax

	;mov eax, esp
	push 0x23
	push 0x00001000
	pushf ;some tric to enable interupt back
	pop eax 
	or eax, 0x200
	push eax
	push 0x1b
	push 0x00000000
	iret
.1:
	;hlt
	jmp .1

section .bss
    align 16
    resb 8192		;8KB for stack
stack_space:

global PAGE_DIRECTORY
global PAGE_TABLE
section .data
    align 4096
PAGE_DIRECTORY:
	dd PAGE_TABLE-0xC0000000+3
	times 767 dd 0
	dd PAGE_TABLE-0xC0000000+3
	times 255 dd 0
PAGE_DIRECTORY_END:
PAGE_TABLE:
	resb 4096
PAGE_TABLE_END: