[bits 16]
[org 0x7C00]

KERNEL_LOAD_SEG     equ 0x1000
KERNEL_LOAD_OFFSET  equ 0x0000
KERNEL_SECTORS      equ 32

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti

    mov [boot_drive], dl

    mov si, msg_loading
    call print_string

    mov ax, KERNEL_LOAD_SEG
    mov es, ax
    xor bx, bx

    mov ah, 0x02              ; BIOS: read sectors
    mov al, KERNEL_SECTORS
    mov ch, 0x00              ; cylinder 0
    mov cl, 0x02              ; sector 2 (sector 1 is this boot sector)
    mov dh, 0x00              ; head 0
    mov dl, [boot_drive]
    int 0x13
    jc disk_error

    jmp KERNEL_LOAD_SEG:KERNEL_LOAD_OFFSET

disk_error:
    mov si, msg_disk_error
    call print_string
.hang:
    hlt
    jmp .hang

print_string:
    lodsb
    test al, al
    jz .done
    mov ah, 0x0E
    mov bh, 0x00
    mov bl, 0x07
    int 0x10
    jmp print_string
.done:
    ret

boot_drive db 0
msg_loading db 'Booting SLOP OS...', 0x0D, 0x0A, 0
msg_disk_error db 'Disk read failure.', 0x0D, 0x0A, 0

times 510-($-$$) db 0
dw 0xAA55
