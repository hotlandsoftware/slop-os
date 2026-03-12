#include "sys.h"

static void write_str(const char *s) {
    (void)sys_write(1, s, cstr_len(s));
}

static void write_spaces(unsigned int count) {
    while (count-- > 0u) {
        (void)sys_write(1, " ", 1u);
    }
}

static void write_u32(unsigned int value) {
    char buf[10];
    unsigned int i = 0u;

    if (value == 0u) {
        (void)sys_write(1, "0", 1u);
        return;
    }

    while (value > 0u && i < sizeof(buf)) {
        buf[i++] = (char)('0' + (value % 10u));
        value /= 10u;
    }

    while (i > 0u) {
        char c = buf[--i];
        (void)sys_write(1, &c, 1u);
    }
}

static void write_cell(unsigned int value, unsigned int width) {
    char buf[10];
    unsigned int i = 0u;
    unsigned int digits;

    if (value == 0u) {
        digits = 1u;
    } else {
        unsigned int tmp = value;
        while (tmp > 0u && i < sizeof(buf)) {
            buf[i++] = (char)('0' + (tmp % 10u));
            tmp /= 10u;
        }
        digits = i;
    }

    if (width > digits) {
        write_spaces(width - digits);
    }

    if (value == 0u) {
        (void)sys_write(1, "0", 1u);
        return;
    }

    while (i > 0u) {
        char c = buf[--i];
        (void)sys_write(1, &c, 1u);
    }
}

static void write_sep(void) {
    write_str(" ");
}

static void write_bar_sep(void) {
    write_str(" | ");
}

static void write_kib_line(const char *label, unsigned int value) {
    write_str(label);
    write_u32(value);
    write_str(" KiB\n");
}

int main(void) {
    struct mem_info info;

    write_str("free format v2\n");

    if (sys_meminfo(&info) < 0) {
        write_str("free: meminfo unavailable\n");
        return 1;
    }

    write_kib_line("MemTotal:      ", info.mem_total_kib);
    write_kib_line("MemUsed:       ", info.mem_used_kib);
    write_kib_line("MemFree:       ", info.mem_free_kib);
    write_kib_line("MemShared:     ", info.mem_shared_kib);
    write_kib_line("BuffersCache:  ", info.mem_buff_cache_kib);
    write_kib_line("MemAvailable:  ", info.mem_available_kib);
    write_kib_line("SwapTotal:     ", info.swap_total_kib);
    write_kib_line("SwapUsed:      ", info.swap_used_kib);
    write_kib_line("SwapFree:      ", info.swap_free_kib);
    return 0;
}
