[bits 32]

section .text
global syscall_stub

extern syscall_entry
extern ring3_active
extern ring3_return_esp
extern ring3_resume_from_user

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
    cmp eax, 240
    jne .dispatch
    cmp dword [ring3_active], 1
    jne .dispatch
    mov dword [ring3_active], 0
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
