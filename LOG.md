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