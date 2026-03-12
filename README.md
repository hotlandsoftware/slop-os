# SLOPerating System
SLOP Lacks Optimal Performance(erating system) is a completely vibecoded operating system prototype that boots into a shell and is being built toward a POSIX-compliant microkernel architecture. 

The goal of this project is simple: to see how far vibecoding has come, and if it can really make something on par with major operating systems.

## Current Goal
- Keep a stable 32-bit kernel baseline that boots reliably on 486-class constraints.
- Move commands from shell builtins into real user binaries.
- Expand syscall/process foundations so userland programs become the default path.
- Preserve simple terminal UX now with GUI groundwork later.

## Current Features
- GRUB Multiboot ISO boot path with a modular freestanding C kernel.
- Basic shell with some commands built in (`help`, `clear`, `cd`, `heap`, `alloc`, `echo`, `panic`, `reboot`, `halt`)
- COM1 serial logging/serial shell context
- Optional framebuffer console path in QEMU with VGA fallback
- Heap allocator
- Kernel panics
- Basic syscall ABI
- Very basic ELF32 support (with ``cat``, ``touch``, ``ls``, ``pwd``, ``mkdir``, ``ps``, and ``systeminfo`` as real ELF files)
- Basic in-memory VFS
- Block device registry + IDE/ATAPI CD + read-only ISO9660 file loading (`/mount/cdrom`)

## Minimum Hardware Profile
- CPU: Intel 80486 (baseline target).
- RAM: 4 MB target baseline.
- Video: text-mode console baseline.

Notes:
- You cannot boot from a floppy (yet), so most real 486 systems probably can't load this
- Tested on v86 and QEMU so far (nothing else)

## Build and Run
Requirements:
- `nasm`
- `make` (or `mingw32-make` on native Windows)
- `i686-elf-gcc`, `i686-elf-ld`
- `grub-mkrescue` (+ backend tooling such as `xorriso`)
- `qemu-system-i386`

Recommended on Windows: build from WSL Ubuntu.

```bash
make
make run
```

Useful targets:
- `make run-lowmem` (lower-RAM boot test path)
- `make iso-fb` / `make run-fb` (framebuffer QEMU path)
- `make run QEMU_SERIAL_OPTS="-serial stdio"` (serial shell on host terminal)
- `make floppy` / `make run-floppy` (legacy floppy boot path)

## FAQ
### Is this production-ready?
Yeah man!

### What do you use?
ChatGPT Codex, Gemini, Claude, and other AI tools.

### Why?
To see how far vibe coding has come.

### Is this really a microkernel?
Directionally yes, currently no. The architecture is moving toward microkernel service boundaries, but most functionality is still in-kernel today.

### Do you have any knowledge of coding, operating systems, etc?
Some, which helps keep experiments constrained and debuggable.
