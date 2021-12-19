bits 32
global _start
extern main

_start:
    call main

    mov ebx, eax
    mov eax, 0x42
    int 0x80 ;call exit syscall