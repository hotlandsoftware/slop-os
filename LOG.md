This file contains a log of the features added (and what day they were added). 
Currently, we are in the "very early" development phase.

---

### Day 1
- Skeleton structure set up. Minimum target hardware (486, 4 MB of RAM, optional hard disk support) established, as well as the decision to use a microkernel.
- Bootloader, Makefile, and initial source (initially Assembly; later switched to C mostly for ease) added.
- Extremely primitive shell added. Successfully boots in QEMU and v86.
- "Windows 95"-like GUI planned - but sticking to a pure console mode for the beginning. 

### Day 2
- Extremely primitive memory management added. Heap allocator is working.
- Primitive VFS support added. Files can now be used and written to RAM.
- Coreutils have been added: ``cd``, ``ls``, ``pwd``, ``mkdir``, ``touch``, ``cat``
- A bug where a ton of CPU was used on idle was fixed.
- Basic interrupt/IRQ work started.
- Serial console support has been added. (mirrors the console right now)
- Basic framebuffer support added with bitmap font & text console with color support
- ``echo`` has been updated to support ``>`` redirect. File writing, baby!

### Day 3
- Block device registry/mount table groundwork has been added.
- Storage + FS init has been added to the kernel.
- Added an initial POSIX-like syscall ABI on `int 0x80` (`read`, `write`, `open`, `close`, `getpid`, `exit` stubs).
- Shell now resolves program-style commands through `/bin/*` execution hooks instead of keeping all commands as direct shell builtins.
- Seeded `/bin` command stubs as groundwork for a future ELF loader and true userspace processes
- Added a dedicated syscall entry stub (`int 0x80`) separate from IRQ stubs with fixed register calling convention.
- Added early task/process table groundwork with PID tracking and round-robin scheduler bookkeeping.
- Added `ps` command to inspect task table state.
- Enabled timer IRQ preemption bookkeeping (IRQ0 unmasked, interrupts enabled) with a fixed scheduler timeslice.
- Added a separate process table layer (`spawn/exit/wait`) to track lifecycle independently of scheduler task slots.
- Updated `ps`/system reporting to show both task scheduler state and process lifecycle state.
- Added ELF32 loader groundwork (header validation + `PT_LOAD` mapping into a reserved user-image buffer).
- Added `run PATH` command and seeded `/bin/hello.elf` test image for initial execution-path testing.
- Added kernel-owned GDT/TSS protection setup and an `iret`-based Ring 3 transition test path.
- Added `ring3test` command that enters CPL3, performs syscall interaction, and returns to kernel control.
- Added kernel panics.

### Day 4
- Added a red-screen kernel panic renderer. Exception panics now show structured details (vector/error/EIP/CS/EFLAGS) on the red screen.
- Added `panic` shell command to trigger a kernel panic for testing.
- Added syscall hardening for Ring 3 user pointers (image/stack range checks, errno-style failures).
- Reworked ELF execution storage from one global buffer to per-process image/stack slots.
- Added argument passing from kernel to user entry (`argc/argv`) on the Ring 3 user stack.
- Real binaries are now a thing! (all binaries in ``/bin/`` are now real ELF32 files.)
- i386 source has been split to better support future architectures.
- Full ISO9660 support added.

New session
---

- ``/bin/`` is now loaded from the CD-ROM.
- ``free`` command (free memory) added.

### Day 5
- Decision made to switch from a microkernel aspiration to a hybrid kernel model for ease

---
- Architecture direction has been clarified: SLOP is now explicitly targeting a hybrid-kernel design instead of forcing an immediate full microkernel split.
- Added the first filesystem dispatch boundary via `fs_ops` tables on mounts, so VFS/mount helpers now call filesystem operations through an interface instead of hardcoding ISO9660 checks everywhere.
- Added the first exec/loader boundary split: ELF parsing/validation now builds an `exec_load_plan`, and process startup consumes that plan separately. (ELF path still runs in kernel, but policy and mechanism are now separated enough to support later service-style refactors.)
- Fixed a bug where binary names were truncated to 8 characters.

### Day 6
- Added cooperative user-task save/resume via `yield`, including saved Ring 3 register state and a resumable user-mode entry path.
- Moved user process run/resume ownership out of `exec.c` and into the task layer, so user-task lifecycle is now managed through task state transitions instead of a one-off exec loop.
- ELF tasks now keep their real command names in the task/process tables
- Separated yielded task lifetime from immediate exec cleanup: a yielded process now remains alive in the task/process tables instead of being reaped right away.
- Added persistence for yielded user images by snapshotting/restoring the active user image, which avoids corruption when another ELF runs before a yielded task resumes.
- Added kernel-owned auto-resume of READY yielded user tasks from the shell idle loop, so resumed user execution is no longer shell-command-driven only.
- User tasks now carry the execution context needed for later scheduler-driven service behavior (`cwd`, output target, saved image/context).
- Added real blocking IPC semantics
- Cleaned up `ps` scheduler/process state reporting so the current task is marked clearly and task/proc states stay in sync better.
- Added named services on top of IPC via a small kernel service registry and user-facing `svc_reg` / `svc_lookup` test tools.
- Added the first real service-style split: `systeminfo` is now an IPC client, and `sysinfod` is a long-lived named service that handles system info requests.
- Minimal libc implementation has been added, allowing VERY basic UNIX programs to run. We have a stripped down ``ed`` that works!