# SLOP - SLOP Lacks Optimal Performance(erating system)
SLOP OS is a vibe-coded operating system prototype that boots from an ISO into a tiny command-line shell.

## Current goal
Bootstrap a minimal system that:
- boots directly into a terminal-like interface
- targets old hardware constraints
- keeps a POSIX-compatible microkernel direction for later growth

## Minimum hardware profile (current)
- CPU: Intel 80486
- RAM: 2 MB
- Video: CGA-era text-mode expectations (currently VGA text mode in QEMU)

The kernel is built with `-march=i486 -mtune=i486` so the generated code matches that CPU target even when using an `i686-elf` cross-toolchain.
GRUB-from-ISO in QEMU is less reliable at exactly `2 MB`, so the default `make run` target uses a little more RAM for development while keeping a separate low-memory run target.

## What exists right now
- Multiboot entry stub (`kernel/boot32.asm`)
- Freestanding C kernel split into small modules under `kernel/`
- Linker script (`linker.ld`)
- GRUB ISO boot config (`iso/boot/grub/grub.cfg`)
- VGA text console command loop with commands:
  - Shell builtins: `help`, `clear`, `cd`, `heap`, `alloc N`, `reboot`, `halt`
  - Program-style `/bin/*` commands: `ls`, `pwd`, `mkdir`, `touch`, `cat`, `echo`, `systeminfo`, `mounts`
- Early watermark heap allocator initialized from Multiboot memory info
- Basic IDT/PIC setup with timer and keyboard IRQ handling
- Early syscall ABI wired on `int 0x80` (`read`, `write`, `open`, `close`, `getpid`, `exit` stubs)
- Tiny in-memory VFS for shell navigation and file inspection
- Storage/filesystem groundwork: block device registry, thin IDE/ATAPI CD probe/read layer, and mount table with `memfs` + `iso9660` placeholder
- COM1 serial logging plus a second shell context on the serial console
- Separate QEMU-focused framebuffer build with VGA text fallback in the shared console layer
- Build and QEMU CD boot targets via `Makefile`
- Legacy real-mode prototype retained in `boot/boot.asm` and `kernel/kernel.asm` (not used in ISO build)

## Project layout (POSIX-oriented direction)
- `boot/` legacy bootloader prototype
- `kernel/` active kernel entry and core shell
- `bin/` planned userland binaries
- `etc/` planned system config
- `usr/` planned secondary hierarchy
- `include/` planned shared headers/interfaces
- `user/` planned userland source
- `docs/` design notes and roadmap
- `iso/` GRUB boot assets for ISO generation

## Implementation language policy
- Assembly: only for boot, mode transitions, interrupt/task-switch stubs, and tiny hardware-critical paths.
- C (primary): kernel logic, memory manager policy, IPC core, syscall layer, and most drivers.
- C++ (optional later): user-space servers/libs after ABI and toolchain are stable.
- Rule: if a feature can be implemented cleanly in C without losing required control, it should not be in assembly.

## Requirements
- `nasm`
- `make` or `mingw32-make`
- `i686-elf-gcc` and `i686-elf-ld` (or compatible freestanding 32-bit toolchain)
- `grub-mkrescue` (plus backend tools such as `xorriso`)
- `qemu-system-i386`

## Build in WSL (recommended on Windows)
If you are developing from Windows, the recommended build path is WSL Ubuntu with the required packages installed there.

```bash
make
```

This avoids the usual Windows friction around `grub-mkrescue` and ISO tooling.

## Build on Windows
Native Windows builds are still supported when you invoke GNU make from `cmd.exe` with the required toolchain on `PATH`.

```bat
mingw32-make
```

If you build from MSYS2, Git Bash, or WSL, plain `make` continues to work as before.

## Build ISO
```bash
make
```

This produces `build/slop.iso`.

For the framebuffer test build:

```bash
make iso-fb
```

This produces `build/slop-fb.iso`.

## Build Floppy Image (legacy boot path)
```bash
make floppy
```

This produces `build/slop-floppy.img` (1.44 MB raw floppy image).
It uses the standalone real-mode legacy path (`boot/boot.asm` + `kernel/kernel.asm`).

## Run in QEMU (486, 2 MB, CD boot)
```bash
make run
```

The default `run` target enables host-friendlier QEMU TCG pacing. To override it, pass your own `QEMU_OPTS=...`.

For the exact low-memory experiment path:

```bash
make run-lowmem
```

For floppy boot:

```bash
make run-floppy
```

To boot from floppy while also attaching the CD image:

```bash
make run-floppy-cd
```

To expose the serial shell on COM1 in QEMU, pass a serial backend such as:

```bash
make run QEMU_SERIAL_OPTS="-serial stdio"
```

For the framebuffer graphics console in QEMU:

```bash
make run-fb
```

The baseline build remains 486-safe, but the default `run` target uses slightly more RAM in QEMU because GRUB ISO boot is unreliable at exactly `2 MB`. The framebuffer path is an optional QEMU-oriented build with higher requirements; real-hardware framebuffer support remains a later roadmap item.

## Next milestones
1. Move from bootstrap shell to protected-mode microkernel service boundaries.
2. Split kernel responsibilities into IPC/scheduler/memory core and user-space servers.
3. Add VFS and process manager servers.
4. Expand POSIX-flavored syscall compatibility and route it over message passing.
5. Expand the interrupt layer beyond the current timer/keyboard path.

# FAQs

## What?
It's a completely vibe coded operating system.

## Why?
To test how far AI-assisted coding can push a systems project.

## What do you use?
ChatGPT Codex, Gemini, Claude, and other AI tooling.

## Should I use it in production?
No.

## Do you have any knowledge of coding, operating systems, etc?
Some, which helps keep experiments constrained and debuggable.
