# Architecture Split

- `kernel/arch/i386/`: active 32-bit x86 implementation.
- `kernel/arch/x86_64/`: reserved for future 64-bit port.

Rule of thumb:
- Put ISA/privilege/interrupt/CPU-setup code in `kernel/arch/<arch>/`.
- Keep architecture-neutral subsystems in `kernel/` (VFS, ELF policy, shell logic, process model, etc.).
