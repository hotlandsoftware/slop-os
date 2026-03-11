[bits 32]

section .text
global gdt_flush
global tss_flush
global enter_user_mode
global ring3_user_stub
global ring3_resume_from_user

extern tss_set_kernel_stack
extern ring3_return_esp
extern ring3_active
extern ring3_kernel_stack_top
extern ring3_saved_ebx
extern ring3_saved_esi
extern ring3_saved_edi
extern ring3_saved_ebp
extern ring3_exit_code

gdt_flush:
    mov eax, [esp + 4]
    lgdt [eax]

    mov ax, [esp + 8]
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    jmp 0x08:.gdt_done
.gdt_done:
    ret

tss_flush:
    mov ax, [esp + 4]
    ltr ax
    ret

enter_user_mode:
    mov [ring3_saved_ebx], ebx
    mov [ring3_saved_esi], esi
    mov [ring3_saved_edi], edi
    mov [ring3_saved_ebp], ebp

    mov esi, [esp + 4]
    mov edi, [esp + 8]

    mov [ring3_return_esp], esp
    mov dword [ring3_active], 1

    mov ecx, [ring3_kernel_stack_top]
    push ecx
    call tss_set_kernel_stack
    add esp, 4

    mov bx, 0x23
    mov ds, bx
    mov es, bx
    mov fs, bx
    mov gs, bx

    push dword 0x23
    push edi
    push dword 0x202
    push dword 0x1B
    push esi
    iretd

ring3_resume_from_user:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ebx, [ring3_saved_ebx]
    mov esi, [ring3_saved_esi]
    mov edi, [ring3_saved_edi]
    mov ebp, [ring3_saved_ebp]
    mov eax, [ring3_exit_code]
    ret

ring3_user_stub:
    mov eax, 240
    xor ebx, ebx
    int 0x80
.hang:
    jmp .hang

section .note.GNU-stack noalloc noexec nowrite progbits
