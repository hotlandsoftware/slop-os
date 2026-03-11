#include "kernel.h"

#define COM1_PORT 0x3F8
#define SERIAL_TX_SPIN_MAX 100000u

static int serial_ready = 0;

static int serial_transmit_empty(void) {
    return (inb(COM1_PORT + 5) & 0x20u) != 0u;
}

static int serial_data_ready(void) {
    return (inb(COM1_PORT + 5) & 0x01u) != 0u;
}

void serial_init(void) {
    outb(COM1_PORT + 1, 0x00);
    outb(COM1_PORT + 3, 0x80);
    outb(COM1_PORT + 0, 0x03);
    outb(COM1_PORT + 1, 0x00);
    outb(COM1_PORT + 3, 0x03);
    outb(COM1_PORT + 2, 0xC7);
    outb(COM1_PORT + 4, 0x0B);

    serial_ready = 1;
}

int serial_is_ready(void) {
    return serial_ready;
}

void serial_putchar(char c) {
    u32 spins = 0;

    if (!serial_ready) {
        return;
    }

    if (c == '\n') {
        serial_putchar('\r');
    }

    while (!serial_transmit_empty()) {
        if (++spins >= SERIAL_TX_SPIN_MAX) {
            return;
        }
    }
    outb(COM1_PORT, (u8)c);
}

void serial_print(const char *s) {
    while (*s) {
        serial_putchar(*s++);
    }
}

int serial_try_read_char(char *out) {
    if (!serial_ready || !serial_data_ready()) {
        return 0;
    }

    *out = (char)inb(COM1_PORT);
    return 1;
}
