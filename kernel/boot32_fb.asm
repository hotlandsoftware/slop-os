[bits 32]

section .multiboot
align 4
    dd 0x1BADB002                ; magic
    dd 0x00000007                ; flags: align modules + memory info + video mode
    dd -(0x1BADB002 + 0x00000007)
    dd 0                         ; header_addr
    dd 0                         ; load_addr
    dd 0                         ; load_end_addr
    dd 0                         ; bss_end_addr
    dd 0                         ; entry_addr
    dd 0                         ; mode_type: linear graphics
    dd 640                       ; width
    dd 480                       ; height
    dd 24                        ; depth

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
