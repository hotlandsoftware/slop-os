# SLOP OS Microkernel Direction

Target model: MINIX-like microkernel with small privileged core and user-space servers.

## Kernel (ring 0)
- Interrupt/exception handling
- Task scheduler and context switch
- IPC primitive (message passing)
- Low-level memory management
- Minimal assembly boundary for direct CPU/hardware-critical routines only

## User-space servers
- Process manager
- VFS/filesystem server
- Device driver servers
- Init/service manager

## POSIX compatibility plan
- User processes call a syscall shim.
- Shim translates calls to IPC messages to the appropriate server.
- Initial compatibility focus: `read`, `write`, `open`, `close`, `fork`, `execve`, `wait`.

## Language boundaries
- Assembly: boot loader, mode switch, interrupt entry/exit stubs, context switch glue, and unavoidable MM/CPU primitives.
- C: default language for kernel and server implementation.
- C++: optional later, mainly for user-space components once ABI/toolchain discipline is in place.
