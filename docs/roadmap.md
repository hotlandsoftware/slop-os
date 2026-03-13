# SLOP OS Roadmap

## Phase 0: Bootstrap (done)
- GRUB loads a multiboot ELF kernel from ISO.
- Assembly is reduced to boot/entry glue (`kernel/arch/i386/boot32.asm`).
- Kernel shell is in freestanding C with VGA text output and interrupt-driven keyboard input.
- Tested profile target remains 486 / 2 MB.

## Phase 1: Early kernel core
- A20 enable + GDT setup.
- Enter 32-bit protected mode.
- Basic interrupt descriptor table. (in progress)
- Timer + keyboard IRQ handling. (in progress)
- Task table + scheduler bookkeeping groundwork in kernel. (in progress)
- Process object model (`spawn/exit/wait`) separated from scheduler task slots. (in progress)
- Ring 3 transition path groundwork via kernel GDT/TSS and `iret` test entry/return. (in progress)
- IPC/request primitive for future user-space services.
- Separate optional QEMU framebuffer text console build. (in progress)
- Storage/filesystem groundwork: block device API, thin IDE/ATAPI CD access layer, mount table, and CD/ISO placeholders. (in progress)

## Phase 2: Hybrid kernel boundary cleanup
- Keep scheduler, interrupt handling, low-level memory management, and core syscall/IPC plumbing in kernel.
- Keep immature or performance-sensitive drivers in kernel until stable service boundaries exist.
- Split subsystems behind explicit interfaces so they can live either in kernel or in user space.
- Continue treating `/bin/*` tools as the primary client surface for new functionality.
- Establish a request/response model that can back either in-kernel services or user-space servers.

## Phase 3: First service split
- Move one non-critical subsystem behind a user-space service boundary end-to-end.
- Preferred first target: filesystem policy/service layer, while low-level block I/O may remain in kernel.
- Add a small process/service manager layer for launching and supervising named services.
- Make kernel-side callers stop reaching directly into high-level service logic.

## Day 5 focus: Hybrid direction lock-in
- Adopt a hybrid-kernel architecture as the near-term target instead of forcing a full microkernel split immediately.
- Define which components are kernel-resident for now: scheduler, protection, basic VM/heap, interrupt core, low-level storage path.
- Define which components should become service-like first: filesystem policy, process management helpers, higher-level device policy, shell-adjacent utilities.
- Add the first IPC/request abstraction that is usable even before services fully move out of kernel.
- Preserve the option to move more subsystems to user space later without rewriting all clients.

## Near-term compatibility goal
- Use BusyBox as the first real-world userland compatibility target.
- Do not target full BusyBox immediately; target a tiny static configuration first.
- First BusyBox-compatible applets to aim for:
  - `sh`
  - `ls`
  - `cat`
  - `echo`
  - `mkdir`
  - `pwd`
- Use those tools as the benchmark for syscall, process, VFS, and terminal behavior.
- Treat hybrid-kernel cleanup and BusyBox compatibility work as linked goals rather than separate tracks.

## Graphics follow-up
- Keep VGA text and QEMU framebuffer console as the current paths.
- Add a future real-hardware graphics path later (VBE or comparable hardware-specific framebuffer setup).

## Phase 4: Userland
- Minimal `/bin/sh` replacement.
- Tiny libc subset.
- Init process and `/etc/inittab`-like config.
- POSIX-flavored syscall compatibility layer over hybrid service boundaries / message passing.
- `int 0x80` syscall ABI scaffold is in place as the first compatibility step, with a dedicated syscall entry stub separated from IRQ stubs.
- Early ELF32 loader path validates `ET_EXEC`/`EM_386` and maps `PT_LOAD` segments into a fixed reserved user-image region.
- Userland build path now produces real `/bin/*` binaries from `user/` sources instead of only embedded command hooks.
- Syscalls now include basic user-pointer validation and per-process user stack slots for execution context.
- BusyBox bring-up target: get a reduced static build running with `sh`, `ls`, `cat`, `echo`, `mkdir`, and `pwd` as the first applets.
