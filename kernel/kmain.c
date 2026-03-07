typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long size_t;

#define VGA_COLS 80
#define VGA_ROWS 25
#define VGA_ATTR 0x07
#define MULTIBOOT_MAGIC 0x2BADB002

struct multiboot_info {
    u32 flags;
    u32 mem_lower;
    u32 mem_upper;
};

extern char _kernel_start;
extern char _kernel_end;

static volatile u16 *const vga = (volatile u16 *)0xB8000;
static u16 cursor_row = 0;
static u16 cursor_col = 0;
static int shift_down = 0;

static inline u8 inb(u16 port) {
    u8 value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline void outb(u16 port, u8 value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static void term_update_hw_cursor(void) {
    u16 pos = (u16)(cursor_row * VGA_COLS + cursor_col);
    outb(0x3D4, 0x0F);
    outb(0x3D5, (u8)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (u8)((pos >> 8) & 0xFF));
}

static void halt_forever(void) {
    for (;;) {
        __asm__ volatile ("cli; hlt");
    }
}

static void term_clear(void) {
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

static void term_putchar(char c) {
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

static void term_print(const char *s) {
    while (*s) {
        term_putchar(*s++);
    }
}

static int str_eq(const char *a, const char *b) {
    while (*a && *b) {
        if (*a != *b) {
            return 0;
        }
        ++a;
        ++b;
    }
    return *a == *b;
}

static int str_startswith(const char *s, const char *prefix) {
    while (*prefix) {
        if (*s != *prefix) {
            return 0;
        }
        ++s;
        ++prefix;
    }
    return 1;
}

static void print_u32_dec(u32 value) {
    char buf[11];
    int i = 0;

    if (value == 0) {
        term_putchar('0');
        return;
    }

    while (value > 0 && i < 10) {
        buf[i++] = (char)('0' + (value % 10));
        value /= 10;
    }

    while (i > 0) {
        term_putchar(buf[--i]);
    }
}

static int cpu_has_cpuid(void) {
    u32 before;
    u32 after;

    __asm__ volatile (
        "pushfl\n"
        "popl %0\n"
        : "=r"(before));

    u32 toggled = before ^ (1u << 21);

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

    return ((before ^ after) & (1u << 21)) != 0;
}

static void cpu_vendor(char out[13]) {
    u32 ebx;
    u32 ecx;
    u32 edx;
    __asm__ volatile (
        "cpuid"
        : "=b"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(0)
        : "cc");

    out[0] = (char)(ebx & 0xFF);
    out[1] = (char)((ebx >> 8) & 0xFF);
    out[2] = (char)((ebx >> 16) & 0xFF);
    out[3] = (char)((ebx >> 24) & 0xFF);
    out[4] = (char)(edx & 0xFF);
    out[5] = (char)((edx >> 8) & 0xFF);
    out[6] = (char)((edx >> 16) & 0xFF);
    out[7] = (char)((edx >> 24) & 0xFF);
    out[8] = (char)(ecx & 0xFF);
    out[9] = (char)((ecx >> 8) & 0xFF);
    out[10] = (char)((ecx >> 16) & 0xFF);
    out[11] = (char)((ecx >> 24) & 0xFF);
    out[12] = '\0';
}

static void print_systeminfo(const struct multiboot_info *mbi, u32 magic) {
    term_print("System Information\n");

    if (magic == MULTIBOOT_MAGIC && mbi && (mbi->flags & 0x1)) {
        u32 conv = mbi->mem_lower;
        u32 ext = mbi->mem_upper;
        term_print("  Conventional RAM: ");
        print_u32_dec(conv);
        term_print(" KiB\n");

        term_print("  Extended RAM:     ");
        print_u32_dec(ext);
        term_print(" KiB\n");

        term_print("  Usable total:     ");
        print_u32_dec(conv + ext);
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
    print_u32_dec((u32)(&_kernel_end - &_kernel_start));
    term_print(" bytes\n\n");
}

static void try_reboot(void) {
    while (inb(0x64) & 0x02) {
    }
    outb(0x64, 0xFE);
    halt_forever();
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

    if (scancode & 0x80) {
        return 0;
    }

    if (scancode >= 128) {
        return 0;
    }

    return shift_down ? shifted[scancode] : normal[scancode];
}

static int keyboard_try_read_char(char *out) {
    if ((inb(0x64) & 0x01) == 0) {
        return 0;
    }

    char c = decode_scancode(inb(0x60));
    if (!c || c == '\t') {
        return 0;
    }

    *out = c;
    return 1;
}

static void cpu_relax_wait(void) {
    volatile u32 i;
    for (i = 0; i < 50000; ++i) {
        __asm__ volatile ("pause");
    }
}

static void shell_loop(const struct multiboot_info *mbi, u32 magic) {
    char input[64];
    u32 len = 0;

    term_print("SLOP OS (microkernel bootstrap)\n");
    term_print("Type \"help\" for commands.\n\n");
    term_print("slop> ");

    for (;;) {
        char c;
        if (!keyboard_try_read_char(&c)) {
            cpu_relax_wait();
            continue;
        }

        if (c == '\n') {
            input[len] = '\0';
            term_putchar('\n');

            if (len == 0) {
                term_print("slop> ");
                continue;
            }

            if (str_eq(input, "help")) {
                term_print("Commands:\n");
                term_print("  help       - list commands\n");
                term_print("  clear      - clear screen\n");
                term_print("  systeminfo - detected CPU/memory info\n");
                term_print("  about      - alias for systeminfo\n");
                term_print("  echo TEXT  - print TEXT\n");
                term_print("  reboot     - reset machine\n");
                term_print("  halt       - stop CPU\n");
            } else if (str_eq(input, "clear")) {
                term_clear();
            } else if (str_eq(input, "systeminfo") || str_eq(input, "about")) {
                print_systeminfo(mbi, magic);
            } else if (str_startswith(input, "echo ")) {
                term_print(input + 5);
                term_putchar('\n');
            } else if (str_eq(input, "reboot")) {
                term_print("Rebooting...\n");
                try_reboot();
            } else if (str_eq(input, "halt")) {
                term_print("CPU halted. Reset VM to continue.\n");
                halt_forever();
            } else {
                term_print("Unknown command. Type \"help\".\n");
            }

            len = 0;
            term_print("slop> ");
            continue;
        }

        if (c == '\b') {
            if (len > 0) {
                --len;
                term_putchar('\b');
            }
            continue;
        }

        if (c >= ' ' && c <= '~') {
            if (len < (sizeof(input) - 1)) {
                input[len++] = c;
                term_putchar(c);
            }
        }
    }
}

void kmain(u32 magic, u32 mbi_addr) {
    const struct multiboot_info *mbi = (const struct multiboot_info *)mbi_addr;
    term_clear();
    shell_loop(mbi, magic);
}
