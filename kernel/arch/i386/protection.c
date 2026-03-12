#include "kernel.h"

struct gdt_entry {
    u16 limit_low;
    u16 base_low;
    u8 base_mid;
    u8 access;
    u8 gran;
    u8 base_high;
} __attribute__((packed));

struct gdt_ptr {
    u16 limit;
    u32 base;
} __attribute__((packed));

struct tss_entry {
    u32 prev_tss;
    u32 esp0;
    u32 ss0;
    u32 esp1;
    u32 ss1;
    u32 esp2;
    u32 ss2;
    u32 cr3;
    u32 eip;
    u32 eflags;
    u32 eax;
    u32 ecx;
    u32 edx;
    u32 ebx;
    u32 esp;
    u32 ebp;
    u32 esi;
    u32 edi;
    u32 es;
    u32 cs;
    u32 ss;
    u32 ds;
    u32 fs;
    u32 gs;
    u32 ldt;
    u16 trap;
    u16 iomap_base;
} __attribute__((packed));

extern void gdt_flush(const struct gdt_ptr *ptr, u16 data_selector);
extern void tss_flush(u16 tss_selector);

static struct gdt_entry gdt[6];
static struct gdt_ptr gdtr;
static struct tss_entry tss;
/* Syscalls running from Ring 3 can traverse filesystem code paths that use
   sizeable local buffers (e.g., ISO sector buffers). Keep this stack large
   enough to avoid corrupting kernel globals. */
static u8 ring3_kernel_stack[32768];

volatile u32 ring3_return_esp = 0;
volatile u32 ring3_active = 0;
volatile u32 ring3_current_pid = 0;
u32 ring3_kernel_stack_top = 0;
u32 ring3_saved_ebx = 0;
u32 ring3_saved_esi = 0;
u32 ring3_saved_edi = 0;
u32 ring3_saved_ebp = 0;
u32 ring3_exit_code = 0;

static void gdt_set_gate(int i, u32 base, u32 limit, u8 access, u8 gran) {
    gdt[i].base_low = (u16)(base & 0xFFFFu);
    gdt[i].base_mid = (u8)((base >> 16) & 0xFFu);
    gdt[i].base_high = (u8)((base >> 24) & 0xFFu);
    gdt[i].limit_low = (u16)(limit & 0xFFFFu);
    gdt[i].gran = (u8)(((limit >> 16) & 0x0Fu) | (gran & 0xF0u));
    gdt[i].access = access;
}

void tss_set_kernel_stack(u32 esp0) {
    tss.esp0 = esp0;
}

void protection_init(void) {
    mem_zero(&gdt[0], sizeof(gdt));
    mem_zero(&tss, sizeof(tss));

    gdt_set_gate(0, 0, 0, 0, 0);
    gdt_set_gate(1, 0, 0xFFFFFu, 0x9Au, 0xCFu);
    gdt_set_gate(2, 0, 0xFFFFFu, 0x92u, 0xCFu);
    gdt_set_gate(3, 0, 0xFFFFFu, 0xFAu, 0xCFu);
    gdt_set_gate(4, 0, 0xFFFFFu, 0xF2u, 0xCFu);
    gdt_set_gate(5, (u32)&tss, (u32)(sizeof(tss) - 1u), 0x89u, 0x00u);

    tss.ss0 = 0x10u;
    ring3_kernel_stack_top = (u32)&ring3_kernel_stack[sizeof(ring3_kernel_stack)];
    tss.esp0 = ring3_kernel_stack_top;
    tss.cs = 0x1Bu;
    tss.ss = 0x23u;
    tss.ds = 0x23u;
    tss.es = 0x23u;
    tss.fs = 0x23u;
    tss.gs = 0x23u;
    tss.iomap_base = (u16)sizeof(tss);

    gdtr.limit = (u16)(sizeof(gdt) - 1u);
    gdtr.base = (u32)&gdt[0];

    gdt_flush(&gdtr, 0x10u);
    tss_flush(0x28u);
}
