#include "kernel.h"

#define IDT_ENTRIES 256
#define PIC1_COMMAND 0x20
#define PIC1_DATA 0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA 0xA1
#define PIC_EOI 0x20
#define IRQ_BASE 0x20
#define KEYBOARD_QUEUE_SIZE 64

struct idt_entry {
    u16 offset_low;
    u16 selector;
    u8 zero;
    u8 type_attr;
    u16 offset_high;
} __attribute__((packed));

struct idt_ptr {
    u16 limit;
    u32 base;
} __attribute__((packed));

extern void idt_load(const struct idt_ptr *ptr);
extern void irq0_stub(void);
extern void irq1_stub(void);
extern void irq_default_stub(void);
extern void irq80_stub(void);

static struct idt_entry idt[IDT_ENTRIES];
static struct idt_ptr idtp;
static volatile u32 tick_count = 0;
static char keyboard_queue[KEYBOARD_QUEUE_SIZE];
static volatile u32 keyboard_head = 0;
static volatile u32 keyboard_tail = 0;
static int shift_down = 0;
static u16 code_selector = 0;

static void io_wait(void) {
    outb(0x80, 0);
}

static void pic_send_eoi(u8 irq) {
    if (irq >= 8u) {
        outb(PIC2_COMMAND, PIC_EOI);
    }
    outb(PIC1_COMMAND, PIC_EOI);
}

static void pic_remap(void) {
    u8 pic1_mask = inb(PIC1_DATA);
    u8 pic2_mask = inb(PIC2_DATA);

    outb(PIC1_COMMAND, 0x11);
    io_wait();
    outb(PIC2_COMMAND, 0x11);
    io_wait();

    outb(PIC1_DATA, IRQ_BASE);
    io_wait();
    outb(PIC2_DATA, IRQ_BASE + 8);
    io_wait();

    outb(PIC1_DATA, 4);
    io_wait();
    outb(PIC2_DATA, 2);
    io_wait();

    outb(PIC1_DATA, 0x01);
    io_wait();
    outb(PIC2_DATA, 0x01);
    io_wait();

    outb(PIC1_DATA, pic1_mask);
    outb(PIC2_DATA, pic2_mask);
}

static void pic_set_mask(u8 irq, int masked) {
    u16 port;
    u8 value;

    if (irq < 8u) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq = (u8)(irq - 8u);
    }

    value = inb(port);
    if (masked) {
        value = (u8)(value | (1u << irq));
    } else {
        value = (u8)(value & ~(1u << irq));
    }
    outb(port, value);
}

static void idt_set_gate(u8 vector, void (*handler)(void)) {
    u32 offset = (u32)handler;
    idt[vector].offset_low = (u16)(offset & 0xFFFFu);
    idt[vector].selector = code_selector;
    idt[vector].zero = 0;
    idt[vector].type_attr = 0x8E;
    idt[vector].offset_high = (u16)((offset >> 16) & 0xFFFFu);
}

static void idt_set_gate_user(u8 vector, void (*handler)(void)) {
    u32 offset = (u32)handler;
    idt[vector].offset_low = (u16)(offset & 0xFFFFu);
    idt[vector].selector = code_selector;
    idt[vector].zero = 0;
    idt[vector].type_attr = 0xEE;
    idt[vector].offset_high = (u16)((offset >> 16) & 0xFFFFu);
}

static void idt_init(void) {
    u32 i;

    __asm__ volatile ("mov %%cs, %0" : "=r"(code_selector));

    for (i = 0; i < IDT_ENTRIES; ++i) {
        idt_set_gate((u8)i, irq_default_stub);
    }

    idt_set_gate(IRQ_BASE + 0, irq0_stub);
    idt_set_gate(IRQ_BASE + 1, irq1_stub);
    idt_set_gate_user(0x80, irq80_stub);

    idtp.limit = (u16)(sizeof(idt) - 1u);
    idtp.base = (u32)&idt[0];
    idt_load(&idtp);
}

static void keyboard_queue_push(char c) {
    u32 next = (keyboard_head + 1u) % KEYBOARD_QUEUE_SIZE;
    if (next == keyboard_tail) {
        return;
    }
    keyboard_queue[keyboard_head] = c;
    keyboard_head = next;
}

static int keyboard_queue_pop(char *out) {
    if (keyboard_head == keyboard_tail) {
        return 0;
    }
    *out = keyboard_queue[keyboard_tail];
    keyboard_tail = (keyboard_tail + 1u) % KEYBOARD_QUEUE_SIZE;
    return 1;
}

static char decode_scancode(u8 scancode) {
    static const char normal[128] = {
        0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', '\t',
        'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0, 'a', 's',
        'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\', 'z', 'x', 'c', 'v',
        'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' ', 0
    };
    static const char shifted[128] = {
        0, 27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b', '\t',
        'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', 0, 'A', 'S',
        'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0, '|', 'Z', 'X', 'C', 'V',
        'B', 'N', 'M', '<', '>', '?', 0, '*', 0, ' ', 0
    };

    if (scancode == 0x2A || scancode == 0x36) {
        shift_down = 1;
        return 0;
    }

    if (scancode == 0xAA || scancode == 0xB6) {
        shift_down = 0;
        return 0;
    }

    if ((scancode & 0x80u) != 0u || scancode >= 128u) {
        return 0;
    }

    return shift_down ? shifted[scancode] : normal[scancode];
}

void interrupt_dispatch(struct interrupt_frame *frame) {
    u8 irq;

    if (frame->vector == 0x80u) {
        syscall_dispatch(frame);
        return;
    }

    if (frame->vector < IRQ_BASE || frame->vector >= IRQ_BASE + 16u) {
        return;
    }

    irq = (u8)(frame->vector - IRQ_BASE);
    if (irq == 0u) {
        ++tick_count;
    } else if (irq == 1u) {
        char c = decode_scancode(inb(0x60));
        if (c != 0 && c != '\t') {
            keyboard_queue_push(c);
        }
    }

    pic_send_eoi(irq);
}

void interrupts_init(void) {
    disable_interrupts();
    pic_remap();
    idt_init();

    pic_set_mask(0, 1);
    pic_set_mask(1, 0);
    pic_set_mask(2, 1);
    pic_set_mask(3, 1);
    pic_set_mask(4, 1);
    pic_set_mask(5, 1);
    pic_set_mask(6, 1);
    pic_set_mask(7, 1);
    pic_set_mask(8, 1);
    pic_set_mask(9, 1);
    pic_set_mask(10, 1);
    pic_set_mask(11, 1);
    pic_set_mask(12, 1);
    pic_set_mask(13, 1);
    pic_set_mask(14, 1);
    pic_set_mask(15, 1);
}

int keyboard_try_read_char(char *out) {
    char c;

    if ((inb(0x64) & 0x01u) == 0u) {
        return 0;
    }

    c = decode_scancode(inb(0x60));
    if (c == 0 || c == '\t') {
        return 0;
    }

    *out = c;
    return 1;
}

int keyboard_read_char_blocking(char *out) {
    for (;;) {
        if (keyboard_try_read_char(out)) {
            return 1;
        }
        cpu_idle();
    }
}

u32 timer_ticks(void) {
    return tick_count;
}
