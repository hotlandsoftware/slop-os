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