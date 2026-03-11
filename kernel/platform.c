#include "kernel.h"

u8 inb(u16 port) {
    u8 value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

void outb(u16 port, u8 value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

u16 inw(u16 port) {
    u16 value;
    __asm__ volatile ("inw %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

void outw(u16 port, u16 value) {
    __asm__ volatile ("outw %0, %1" : : "a"(value), "Nd"(port));
}

void halt_forever(void) {
    for (;;) {
        __asm__ volatile ("cli; hlt");
    }
}

void enable_interrupts(void) {
    __asm__ volatile ("sti");
}

void disable_interrupts(void) {
    __asm__ volatile ("cli");
}

void cpu_idle(void) {
    __asm__ volatile ("hlt");
}

void cpu_relax_wait(void) {
    volatile u32 i;
    for (i = 0; i < 50000u; ++i) {
        __asm__ volatile ("nop");
    }
}

void try_reboot(void) {
    while (inb(0x64) & 0x02u) {
    }
    outb(0x64, 0xFE);
    halt_forever();
}

int cpu_has_cpuid(void) {
    u32 before;
    u32 after;
    u32 toggled;

    __asm__ volatile (
        "pushfl\n"
        "popl %0\n"
        : "=r"(before));

    toggled = before ^ (1u << 21);

    __asm__ volatile (
        "pushl %0\n"
        "popfl\n"
        :
        : "r"(toggled)
        : "cc");

    __asm__ volatile (
        "pushfl\n"
        "popl %0\n"
        : "=r"(after));

    __asm__ volatile (
        "pushl %0\n"
        "popfl\n"
        :
        : "r"(before)
        : "cc");

    return ((before ^ after) & (1u << 21)) != 0u;
}

void cpu_vendor(char out[13]) {
    u32 ebx;
    u32 ecx;
    u32 edx;

    __asm__ volatile (
        "cpuid"
        : "=b"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(0)
        : "cc");

    out[0] = (char)(ebx & 0xFFu);
    out[1] = (char)((ebx >> 8) & 0xFFu);
    out[2] = (char)((ebx >> 16) & 0xFFu);
    out[3] = (char)((ebx >> 24) & 0xFFu);
    out[4] = (char)(edx & 0xFFu);
    out[5] = (char)((edx >> 8) & 0xFFu);
    out[6] = (char)((edx >> 16) & 0xFFu);
    out[7] = (char)((edx >> 24) & 0xFFu);
    out[8] = (char)(ecx & 0xFFu);
    out[9] = (char)((ecx >> 8) & 0xFFu);
    out[10] = (char)((ecx >> 16) & 0xFFu);
    out[11] = (char)((ecx >> 24) & 0xFFu);
    out[12] = '\0';
}

void print_systeminfo(const struct multiboot_info *mbi, u32 magic) {
    term_print("System Information\n");

    if (magic == MULTIBOOT_MAGIC && mbi && (mbi->flags & 0x1u)) {
        term_print("  Conventional RAM: ");
        term_print_u32_dec(mbi->mem_lower);
        term_print(" KiB\n");

        term_print("  Extended RAM:     ");
        term_print_u32_dec(mbi->mem_upper);
        term_print(" KiB\n");

        term_print("  Usable total:     ");
        term_print_u32_dec(mbi->mem_lower + mbi->mem_upper);
        term_print(" KiB\n");
    } else {
        term_print("  RAM info: unavailable from multiboot\n");
    }

    term_print("  CPU:              ");
    if (cpu_has_cpuid()) {
        char vendor[13];
        cpu_vendor(vendor);
        term_print("CPUID vendor: ");
        term_print(vendor);
    } else {
        term_print("80486-class (no CPUID)");
    }
    term_print("\n");

    term_print("  Kernel image:     ");
    term_print_u32_dec((u32)(&_kernel_end - &_kernel_start));
    term_print(" bytes\n");

    term_print("  Console backend:  ");
    if (console_has_framebuffer()) {
        term_print("framebuffer ");
        term_print_u32_dec(console_width());
        term_print("x");
        term_print_u32_dec(console_height());
        term_print("\n");
    } else {
        term_print("VGA text mode\n");
    }

    term_print("  Timer ticks:      ");
    term_print_u32_dec(timer_ticks());
    term_print(" (IRQ0 preemption)\n");

    term_print("  Heap state:       ");
    if (heap_is_ready()) {
        term_print_u32_dec(heap_bytes_used());
        term_print(" / ");
        term_print_u32_dec(heap_bytes_total());
        term_print(" bytes used\n");
    } else {
        term_print("unavailable\n");
    }

    term_print("  Block devices:    ");
    term_print_u32_dec(storage_device_count());
    term_print("\n");

    term_print("  Mount entries:    ");
    term_print_u32_dec(fs_mount_count());
    term_print("\n");

    term_print("  Tasks:            ");
    term_print_u32_dec(task_count());
    term_print("\n");

    term_print("  Processes:        ");
    term_print_u32_dec(proc_count());
    term_print("\n");

    term_print("  Sched switches:   ");
    term_print_u32_dec(scheduler_switch_count());
    term_print("\n");

    term_putchar('\n');
}
