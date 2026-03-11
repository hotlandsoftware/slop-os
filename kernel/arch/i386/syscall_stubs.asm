[bits 32]

section .text
global syscall_stub

extern syscall_entry
extern ring3_active
extern ring3_current_pid
extern ring3_return_esp
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
    cmp dword [ring3_active], 1
    jne .dispatch
    cmp eax, 60
    je .return_from_user_exit
    cmp eax, 240
    jne .dispatch
    mov dword [ring3_exit_code], 0
    jmp .return_to_kernel

.return_from_user_exit:
    mov ecx, [esp + 16]
    mov [ring3_exit_code], ecx

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
