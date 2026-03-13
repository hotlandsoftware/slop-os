# `ed` Port Plan

## Goal

Get a real external `ed` binary running on SLOP before attempting BusyBox.

This is a smaller and more realistic hosted-program target than BusyBox or
`vi`, while still exercising real Unix-style program behavior:

- command-line parsing
- stdin/stdout interaction
- file open/read/write/close
- error reporting
- nontrivial internal buffer logic

## Why `ed` first

`ed` is a good stepping stone because:

- it is much smaller than BusyBox
- it is line-oriented, so terminal demands are simpler
- it is a classic Unix program with a narrow surface area
- it forces us to tighten userland compatibility without requiring a full shell

## Likely source choice

The suggested starting point is GNU `ed`.

Repository referenced for investigation:

- `https://github.com/happy5214/gnu-ed`

We have not vendored it into this repo yet. The likely next step is to inspect
its source and decide whether it is practical to adapt directly or whether a
smaller historical `ed` codebase would be easier.

## Build strategy

To keep normal kernel builds simple, external ports should not become part of
the default compile pipeline immediately.

Instead:

1. Port the program in a separate environment/toolchain.
2. Build a final freestanding or SLOP-targeted `ed.elf`.
3. Drop that binary into `third_party/prebuilt/ed.elf`.
4. Let the normal ISO build include it automatically as `/bin/ed`.

This keeps:

- `make`
- `make run`

simple and fast for normal kernel work.

## What the port must adapt to

Before GNU `ed` can run, it will probably need adaptation around:

- libc assumptions
- file descriptor semantics
- error/errno behavior
- terminal input behavior
- any host OS dependencies in its build system

The simplest path is likely:

- replace host-specific libc/syscall calls with a thin SLOP compatibility layer
- build a dedicated `ed.elf` once
- ship it as a prebuilt binary first

## Immediate next checks

1. Inspect GNU `ed` source layout and build dependencies.
2. Identify the minimum libc/syscall surface it expects.
3. Decide whether to:
   - patch GNU `ed` directly, or
   - port a smaller `ed` implementation first
4. Produce a first externally built `ed.elf` and stage it in
   `third_party/prebuilt/`.
