[bits 32]

section .multiboot
align 4
    dd 0x1BADB002                ; magic
    dd 0x00000003                ; flags: align modules + memory info
    dd -(0x1BADB002 + 0x00000003)

section .bss
align 16
stack_bottom:
    resb 16384
stack_top:

section .text
global _start
extern kmain

_start:
    mov esp, stack_top

    ; Multiboot: EAX=magic, EBX=multiboot_info pointer
    push ebx
    push eax
    call kmain

.hang:
    cli
    hlt
    jmp .hang
