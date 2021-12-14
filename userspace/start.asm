bits 32
global _start
extern main

_start:
    mov esp, stack_space
    call main

loop:
    pause
    jmp loop

section .bss
    align 16
    resb 8192
stack_space: