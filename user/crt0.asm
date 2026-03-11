[bits 32]

section .text
global _start
extern main

_start:
    mov eax, [esp]
    mov ebx, [esp + 4]
    push ebx
    push eax
    call main
    add esp, 8
    mov ebx, eax
    mov eax, 60
    int 0x80

.hang:
    jmp .hang

section .note.GNU-stack noalloc noexec nowrite progbits
