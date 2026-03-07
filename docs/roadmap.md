# SLOP OS Roadmap

## Phase 0: Bootstrap (done)
- GRUB loads a multiboot ELF kernel from ISO.
- Assembly is reduced to boot/entry glue (`kernel/boot32.asm`).
- Kernel shell is in freestanding C (`kernel/kmain.c`) with VGA text output and polled keyboard input.
- Tested profile target remains 486 / 2 MB.

## Phase 1: Early microkernel core
- A20 enable + GDT setup.
- Enter 32-bit protected mode.
- Basic interrupt descriptor table.
- Timer + keyboard IRQ handling.
- IPC primitive for user-space servers.

## Phase 2: Server split (MINIX-like direction)
- Scheduler + low-level memory management remain in kernel.
- File system server, process manager, and driver model moved to user space.
- Message-passing API between tasks and servers.

## Phase 3: Userland
- Minimal `/bin/sh` replacement.
- Tiny libc subset.
- Init process and `/etc/inittab`-like config.
- POSIX-flavored syscall compatibility layer over message passing.
