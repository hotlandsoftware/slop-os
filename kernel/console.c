#include "kernel.h"

#define VGA_COLS 80
#define VGA_ROWS 25
#define VGA_ATTR 0x07

static volatile u16 *const vga = (volatile u16 *)0xB8000;
static u16 cursor_row = 0;
static u16 cursor_col = 0;

static void term_enable_hw_cursor(u8 start, u8 end) {
    outb(0x3D4, 0x0A);
    outb(0x3D5, (u8)((inb(0x3D5) & 0xC0u) | start));
    outb(0x3D4, 0x0B);
    outb(0x3D5, (u8)((inb(0x3D5) & 0xE0u) | end));
}

static void term_update_hw_cursor(void) {
    u16 pos = (u16)(cursor_row * VGA_COLS + cursor_col);
    outb(0x3D4, 0x0F);
    outb(0x3D5, (u8)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (u8)((pos >> 8) & 0xFF));
}

void term_init(void) {
    term_enable_hw_cursor(14, 15);
    term_update_hw_cursor();
}

void term_clear(void) {
    u16 i;
    for (i = 0; i < VGA_COLS * VGA_ROWS; ++i) {
        vga[i] = ((u16)VGA_ATTR << 8) | ' ';
    }
    cursor_row = 0;
    cursor_col = 0;
    term_update_hw_cursor();
}

static void term_scroll_if_needed(void) {
    u16 row;
    u16 col;

    if (cursor_row < VGA_ROWS) {
        return;
    }

    for (row = 1; row < VGA_ROWS; ++row) {
        for (col = 0; col < VGA_COLS; ++col) {
            vga[(row - 1) * VGA_COLS + col] = vga[row * VGA_COLS + col];
        }
    }

    for (col = 0; col < VGA_COLS; ++col) {
        vga[(VGA_ROWS - 1) * VGA_COLS + col] = ((u16)VGA_ATTR << 8) | ' ';
    }

    cursor_row = VGA_ROWS - 1;
}

void term_putchar(char c) {
    if (c == '\n') {
        cursor_col = 0;
        ++cursor_row;
        term_scroll_if_needed();
        term_update_hw_cursor();
        return;
    }

    if (c == '\r') {
        cursor_col = 0;
        term_update_hw_cursor();
        return;
    }

    if (c == '\b') {
        if (cursor_col > 0) {
            --cursor_col;
            vga[cursor_row * VGA_COLS + cursor_col] = ((u16)VGA_ATTR << 8) | ' ';
        }
        term_update_hw_cursor();
        return;
    }

    vga[cursor_row * VGA_COLS + cursor_col] = ((u16)VGA_ATTR << 8) | (u8)c;
    ++cursor_col;

    if (cursor_col >= VGA_COLS) {
        cursor_col = 0;
        ++cursor_row;
        term_scroll_if_needed();
    }

    term_update_hw_cursor();
}

void term_print(const char *s) {
    while (*s) {
        term_putchar(*s++);
    }
}

void term_print_u32_dec(u32 value) {
    char buf[11];
    int i = 0;

    if (value == 0) {
        term_putchar('0');
        return;
    }

    while (value > 0 && i < 10) {
        buf[i++] = (char)('0' + (value % 10u));
        value /= 10u;
    }

    while (i > 0) {
        term_putchar(buf[--i]);
    }
}

static void term_print_hex_nibble(u8 value) {
    value &= 0x0Fu;
    if (value < 10u) {
        term_putchar((char)('0' + value));
    } else {
        term_putchar((char)('A' + (value - 10u)));
    }
}

void term_print_hex_u32(u32 value) {
    int shift;
    term_print("0x");
    for (shift = 28; shift >= 0; shift -= 4) {
        term_print_hex_nibble((u8)((value >> shift) & 0x0Fu));
    }
}

void term_vprintf(const char *fmt, va_list args) {
    while (*fmt) {
        if (*fmt != '%') {
            term_putchar(*fmt++);
            continue;
        }

        ++fmt;
        if (*fmt == '\0') {
            break;
        }

        if (*fmt == '%') {
            term_putchar('%');
        } else if (*fmt == 'c') {
            term_putchar((char)va_arg(args, int));
        } else if (*fmt == 's') {
            const char *s = va_arg(args, const char *);
            term_print(s ? s : "(null)");
        } else if (*fmt == 'u') {
            term_print_u32_dec(va_arg(args, u32));
        } else if (*fmt == 'x') {
            term_print_hex_u32(va_arg(args, u32));
        } else {
            term_putchar('%');
            term_putchar(*fmt);
        }

        ++fmt;
    }
}

void term_printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    term_vprintf(fmt, args);
    va_end(args);
}
