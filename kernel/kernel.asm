[bits 16]
[org 0x0000]

start:
    cli
    mov ax, cs
    mov ds, ax
    mov es, ax
    mov ax, 0x9000
    mov ss, ax
    mov sp, 0xFFFE
    sti

    call clear_screen
    mov si, banner
    call print_string

shell:
    mov si, prompt
    call print_string

    mov di, input_buf
    mov cx, INPUT_BUF_LEN
    call read_line

    cmp byte [input_buf], 0
    je shell

    mov si, input_buf
    mov di, cmd_help
    call str_eq
    jc do_help

    mov si, input_buf
    mov di, cmd_clear
    call str_eq
    jc do_clear

    mov si, input_buf
    mov di, cmd_systeminfo
    call str_eq
    jc do_systeminfo

    mov si, input_buf
    mov di, cmd_about
    call str_eq
    jc do_systeminfo

    mov si, input_buf
    mov di, cmd_reboot
    call str_eq
    jc do_reboot

    mov si, input_buf
    mov di, cmd_halt
    call str_eq
    jc do_halt

    mov si, input_buf
    mov di, cmd_echo_prefix
    call str_startswith
    jc do_echo

    mov si, msg_unknown
    call print_string
    jmp shell

do_help:
    mov si, msg_help
    call print_string
    jmp shell

do_clear:
    call clear_screen
    jmp shell

do_systeminfo:
    call print_systeminfo
    jmp shell

do_echo:
    mov si, input_buf + 5
    call print_string
    call print_newline
    jmp shell

do_reboot:
    mov si, msg_reboot
    call print_string
    int 0x19
    jmp $

do_halt:
    mov si, msg_halt
    call print_string
.halt_loop:
    cli
    hlt
    jmp .halt_loop

; -----------------------------
; IO helpers
; -----------------------------

clear_screen:
    mov ax, 0x0003
    int 0x10
    ret

print_newline:
    mov al, 0x0D
    call print_char
    mov al, 0x0A
    call print_char
    ret

print_char:
    mov ah, 0x0E
    mov bh, 0x00
    mov bl, 0x07
    int 0x10
    ret

print_string:
    lodsb
    test al, al
    jz .done
    call print_char
    jmp print_string
.done:
    ret

print_u16_dec:
    ; IN: AX = unsigned value
    push ax
    push bx
    push cx
    push dx

    cmp ax, 0
    jne .convert
    mov al, '0'
    call print_char
    jmp .done

.convert:
    mov bx, 10
    xor cx, cx

.loop:
    xor dx, dx
    div bx
    push dx
    inc cx
    test ax, ax
    jne .loop

.emit:
    pop dx
    add dl, '0'
    mov al, dl
    call print_char
    loop .emit

.done:
    pop dx
    pop cx
    pop bx
    pop ax
    ret

print_u8_hex:
    ; IN: AL = byte to print as two hex chars
    push ax
    push bx

    mov bl, al
    shr al, 4
    call print_hex_nibble

    mov al, bl
    and al, 0x0F
    call print_hex_nibble

    pop bx
    pop ax
    ret

print_hex_nibble:
    ; IN: AL = value 0..15
    and al, 0x0F
    cmp al, 9
    jbe .digit
    add al, 7
.digit:
    add al, '0'
    call print_char
    ret

print_systeminfo:
    push ax
    push bx
    push cx
    push dx
    push si

    mov si, msg_systeminfo_header
    call print_string

    ; Conventional memory in KiB (below 1 MiB).
    int 0x12
    mov [mem_conv_kib], ax

    ; Extended memory in KiB above 1 MiB.
    mov ah, 0x88
    int 0x15
    jc .ext_mem_fail
    mov [mem_ext_kib], ax
    jmp .memory_done

.ext_mem_fail:
    mov word [mem_ext_kib], 0

.memory_done:
    mov si, msg_mem_conventional
    call print_string
    mov ax, [mem_conv_kib]
    call print_u16_dec
    mov si, msg_kib_newline
    call print_string

    mov si, msg_mem_extended
    call print_string
    mov ax, [mem_ext_kib]
    call print_u16_dec
    mov si, msg_kib_newline
    call print_string

    mov si, msg_mem_usable
    call print_string
    mov ax, [mem_conv_kib]
    add ax, [mem_ext_kib]
    call print_u16_dec
    mov si, msg_kib_newline
    call print_string

    mov si, msg_cpu
    call print_string
    call detect_cpu
    call print_newline

    mov si, msg_kernel_bytes
    call print_string
    mov ax, KERNEL_IMAGE_BYTES
    call print_u16_dec
    call print_newline

    mov si, msg_stack_window
    call print_string

    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

detect_cpu:
    ; 386 vs 486+ check via EFLAGS.AC (bit 18).
    ; CPUID availability check via EFLAGS.ID (bit 21).
    push eax
    push ebx
    push ecx
    push edx

    pushfd
    pop eax
    mov ecx, eax
    xor eax, (1 << 18)
    push eax
    popfd

    pushfd
    pop eax
    xor eax, ecx
    and eax, (1 << 18)
    push ecx
    popfd
    jz .cpu_386

    ; 486+ path
    pushfd
    pop eax
    mov ecx, eax
    xor eax, (1 << 21)
    push eax
    popfd

    pushfd
    pop eax
    xor eax, ecx
    and eax, (1 << 21)
    push ecx
    popfd
    jz .cpu_486_no_cpuid

    ; CPUID available: show vendor string.
    xor eax, eax
    cpuid
    mov dword [cpu_vendor + 0], ebx
    mov dword [cpu_vendor + 4], edx
    mov dword [cpu_vendor + 8], ecx
    mov byte [cpu_vendor + 12], 0

    mov si, msg_cpu_cpuid
    call print_string
    mov si, cpu_vendor
    call print_string
    jmp .done

.cpu_386:
    mov si, msg_cpu_386
    call print_string
    jmp .done

.cpu_486_no_cpuid:
    mov si, msg_cpu_486
    call print_string

.done:
    pop edx
    pop ecx
    pop ebx
    pop eax
    ret

read_line:
    ; IN: DI = destination buffer, CX = max buffer size (including null terminator)
    ; OUT: zero-terminated line in buffer
    push ax
    push bx
    push cx
    push dx

    xor bx, bx              ; current length
    dec cx                  ; keep room for terminator

.read_char:
    xor ah, ah
    int 0x16                ; BIOS keyboard read

    cmp al, 0x0D            ; Enter
    je .done

    cmp al, 0x08            ; Backspace
    jne .regular_char

    cmp bx, 0
    je .read_char

    dec bx
    dec di

    mov al, 0x08
    call print_char
    mov al, ' '
    call print_char
    mov al, 0x08
    call print_char
    jmp .read_char

.regular_char:
    cmp bx, cx
    jae .read_char          ; ignore extra chars beyond limit

    stosb
    inc bx
    call print_char
    jmp .read_char

.done:
    mov al, 0
    stosb
    call print_newline

    pop dx
    pop cx
    pop bx
    pop ax
    ret

; -----------------------------
; String helpers
; -----------------------------

str_eq:
    ; Compare zero-terminated SI and DI strings.
    ; Carry set if equal.
.next:
    mov al, [si]
    mov ah, [di]
    cmp al, ah
    jne .not_equal

    test al, al
    je .equal

    inc si
    inc di
    jmp .next

.equal:
    stc
    ret

.not_equal:
    clc
    ret

str_startswith:
    ; Carry set if string at SI starts with prefix at DI.
.next:
    mov ah, [di]
    test ah, ah
    je .match

    mov al, [si]
    cmp al, ah
    jne .no_match

    inc si
    inc di
    jmp .next

.match:
    stc
    ret

.no_match:
    clc
    ret

; -----------------------------
; Data
; -----------------------------

INPUT_BUF_LEN equ 64
KERNEL_IMAGE_BYTES equ kernel_end - $$

banner db 'SLOP OS (486 profile)', 0x0D, 0x0A
       db 'POSIX-style layout, tiny shell, BIOS console', 0x0D, 0x0A
       db 'Type "help" for commands.', 0x0D, 0x0A, 0x0D, 0x0A, 0

prompt db 'slop> ', 0

msg_help db 'Commands:', 0x0D, 0x0A
         db '  help      - list commands', 0x0D, 0x0A
         db '  clear     - clear screen', 0x0D, 0x0A
         db '  systeminfo - detected CPU/memory info', 0x0D, 0x0A
         db '  about     - alias for systeminfo', 0x0D, 0x0A
         db '  echo TEXT - print TEXT', 0x0D, 0x0A
         db '  reboot    - warm reboot', 0x0D, 0x0A
         db '  halt      - stop CPU', 0x0D, 0x0A, 0

msg_systeminfo_header db 'System Information', 0x0D, 0x0A, 0
msg_mem_conventional db '  Conventional RAM: ', 0
msg_mem_extended db '  Extended RAM:     ', 0
msg_mem_usable db '  Usable total:     ', 0
msg_kib_newline db ' KiB', 0x0D, 0x0A, 0
msg_cpu db '  CPU:              ', 0
msg_cpu_386 db '80386-class (no AC flag toggle)', 0
msg_cpu_486 db '80486-class (no CPUID)', 0
msg_cpu_cpuid db 'CPUID vendor: ', 0
msg_kernel_bytes db '  Kernel image:     ', 0
msg_stack_window db '  Stack window:     64 KiB at 0x9000:0000', 0x0D, 0x0A, 0x0D, 0x0A, 0

msg_unknown db 'Unknown command. Type "help".', 0x0D, 0x0A, 0
msg_reboot db 'Rebooting...', 0x0D, 0x0A, 0
msg_halt db 'CPU halted. Reset VM to continue.', 0x0D, 0x0A, 0

cmd_help db 'help', 0
cmd_clear db 'clear', 0
cmd_systeminfo db 'systeminfo', 0
cmd_about db 'about', 0
cmd_echo_prefix db 'echo ', 0
cmd_reboot db 'reboot', 0
cmd_halt db 'halt', 0

mem_conv_kib dw 0
mem_ext_kib dw 0
cpu_vendor times 13 db 0
input_buf times INPUT_BUF_LEN db 0

kernel_end:
