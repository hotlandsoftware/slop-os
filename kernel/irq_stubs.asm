[bits 32]

section .text
global idt_load
global irq0_stub
global irq1_stub
global irq_default_stub

extern interrupt_dispatch

idt_load:
    mov eax, [esp + 4]
    lidt [eax]
    ret

%macro IRQ_STUB 2
%1:
    push dword %2
    jmp irq_common
%endmacro

IRQ_STUB irq_default_stub, 0xFF
IRQ_STUB irq0_stub, 0x20
IRQ_STUB irq1_stub, 0x21

irq_common:
    push eax
    push ecx
    push edx
    push ebx
    push esp
    push ebp
    push esi
    push edi
    push ds
    push es
    push fs
    push gs

    push esp
    call interrupt_dispatch
    add esp, 4

    pop gs
    pop fs
    pop es
    pop ds
    pop edi
    pop esi
    pop ebp
    add esp, 4
    pop ebx
    pop edx
    pop ecx
    pop eax
    add esp, 4
    iretd

section .note.GNU-stack noalloc noexec nowrite progbits
