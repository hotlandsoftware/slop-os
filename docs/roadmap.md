# SLOP OS Roadmap

## Phase 0: Bootstrap (done)
- GRUB loads a multiboot ELF kernel from ISO.
- Assembly is reduced to boot/entry glue (`kernel/boot32.asm`).
- Kernel shell is in freestanding C with VGA text output and interrupt-driven keyboard input.
- Tested profile target remains 486 / 2 MB.

## Phase 1: Early microkernel core
- A20 enable + GDT setup.
- Enter 32-bit protected mode.
- Basic interrupt descriptor table. (in progress)
- Timer + keyboard IRQ handling. (in progress)
- IPC primitive for user-space servers.
- Separate optional QEMU framebuffer text console build. (in progress)

## Phase 2: Server split (MINIX-like direction)
- Scheduler + low-level memory management remain in kernel.
- File system server, process manager, and driver model moved to user space.
- Message-passing API between tasks and servers.

## Graphics follow-up
- Keep VGA text and QEMU framebuffer console as the current paths.
- Add a future real-hardware graphics path later (VBE or comparable hardware-specific framebuffer setup).

## Phase 3: Userland
- Minimal `/bin/sh` replacement.
- Tiny libc subset.
- Init process and `/etc/inittab`-like config.
- POSIX-flavored syscall compatibility layer over message passing.
