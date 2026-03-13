[bits 32]

section .text
global syscall_stub

extern syscall_entry
extern syscall_save_yield_context
extern ring3_active
extern ring3_current_pid
extern ring3_return_esp
extern ring3_stop_reason
extern ring3_resume_from_user
extern ring3_exit_code

; Syscall ABI (int 0x80):
;   eax = syscall number
;   ebx, ecx, edx, esi, edi = args 1..5
; Return:
;   eax = return value (negative for errors)
; Clobbers:
;   eax only (all other GPRs preserved by this stub)
syscall_stub:
    pushad

    mov eax, [esp + 28]
    mov ecx, [esp + 36]
    and ecx, 0x3
    cmp eax, 60
    je .return_from_user_exit
    cmp eax, 240
    je .return_ret_kernel
    cmp eax, 248
    je .return_yield
    jmp .dispatch

.return_from_user_exit:
    cmp ecx, 0x3
    je .return_from_user_exit_do
    cmp dword [ring3_active], 1
    jne .dispatch
.return_from_user_exit_do:
    mov ecx, [esp + 16]
    mov [ring3_exit_code], ecx
    jmp .return_to_kernel

.return_ret_kernel:
    cmp ecx, 0x3
    je .return_ret_kernel_do
    cmp dword [ring3_active], 1
    jne .dispatch
.return_ret_kernel_do:
    mov dword [ring3_exit_code], 0
    mov dword [ring3_stop_reason], 2
    jmp .return_to_kernel

.return_yield:
    cmp ecx, 0x3
    je .return_yield_do
    cmp dword [ring3_active], 1
    jne .dispatch
.return_yield_do:
    push esp
    call syscall_save_yield_context
    add esp, 4
    mov dword [ring3_exit_code], 0
    mov dword [ring3_stop_reason], 1

.return_to_kernel:
    mov dword [ring3_active], 0
    mov dword [ring3_current_pid], 0
    mov esp, [ring3_return_esp]
    jmp ring3_resume_from_user

.dispatch:
    mov ebx, [esp + 16]
    mov ecx, [esp + 24]
    mov edx, [esp + 20]
    mov esi, [esp + 4]
    mov edi, [esp + 0]

    push edi
    push esi
    push edx
    push ecx
    push ebx
    push eax
    call syscall_entry
    add esp, 24

.normal_return:
    mov [esp + 28], eax
    popad
    iretd

section .note.GNU-stack noalloc noexec nowrite progbits
