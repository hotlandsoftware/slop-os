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

## What exists right now
- Multiboot entry stub (`kernel/boot32.asm`)
- Freestanding C kernel shell (`kernel/kmain.c`)
- Linker script (`linker.ld`)
- GRUB ISO boot config (`iso/boot/grub/grub.cfg`)
- VGA text console command loop with commands:
  - `help`
  - `clear`
  - `systeminfo` (runtime-detected RAM/CPU/kernel footprint)
  - `about` (alias of `systeminfo`)
  - `echo TEXT`
  - `reboot`
  - `halt`
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
- `make`
- `i686-elf-gcc` and `i686-elf-ld` (or compatible freestanding 32-bit toolchain)
- `grub-mkrescue` (plus backend tools such as `xorriso`)
- `qemu-system-i386`

## Build ISO
```bash
make
```

This produces `build/slop.iso`.

## Run in QEMU (486, 2 MB, CD boot)
```bash
make run
```

## Next milestones
1. Move from bootstrap shell to protected-mode microkernel service boundaries.
2. Split kernel responsibilities into IPC/scheduler/memory core and user-space servers.
3. Add VFS and process manager servers.
4. Introduce POSIX-flavored syscall compatibility over message passing.
5. Replace polling keyboard path with interrupt-driven input.

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
