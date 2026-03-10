#include "kernel.h"

#define VGA_COLS 80
#define VGA_ROWS 25
#define FONT_W 8
#define FONT_H 8
#define CELL_H 16
#define MAX_CONSOLE_COLS 120
#define MAX_CONSOLE_ROWS 60

enum display_backend {
    DISPLAY_VGA_TEXT = 0,
    DISPLAY_FRAMEBUFFER = 1
};

struct framebuffer_info {
    volatile u8 *base;
    u32 pitch;
    u32 width;
    u32 height;
    u8 bpp;
    u8 type;
    u8 red_pos;
    u8 red_mask;
    u8 green_pos;
    u8 green_mask;
    u8 blue_pos;
    u8 blue_mask;
};

static volatile u16 *const vga = (volatile u16 *)0xB8000;
static enum display_backend backend = DISPLAY_VGA_TEXT;
static struct framebuffer_info fb;
static u16 cursor_row = 0;
static u16 cursor_col = 0;
static u16 text_rows = VGA_ROWS;
static u16 text_cols = VGA_COLS;
static char text_chars[MAX_CONSOLE_COLS * MAX_CONSOLE_ROWS];
static u8 text_fg[MAX_CONSOLE_COLS * MAX_CONSOLE_ROWS];
static u8 text_bg[MAX_CONSOLE_COLS * MAX_CONSOLE_ROWS];
static u8 current_fg = 7;
static u8 current_bg = 0;

static const u32 palette[16] = {
    0x000000u, 0x0000AAu, 0x00AA00u, 0x00AAAAu,
    0xAA0000u, 0xAA00AAu, 0xAA5500u, 0xAAAAAAu,
    0x555555u, 0x5555FFu, 0x55FF55u, 0x55FFFFu,
    0xFF5555u, 0xFF55FFu, 0xFFFF55u, 0xFFFFFFu
};

static const u8 glyph_space[7] = {0, 0, 0, 0, 0, 0, 0};
static const u8 glyph_unknown[7] = {14, 17, 1, 2, 4, 0, 4};
static const u8 glyph_dash[7] = {0, 0, 0, 31, 0, 0, 0};
static const u8 glyph_dot[7] = {0, 0, 0, 0, 0, 12, 12};
static const u8 glyph_slash[7] = {1, 2, 4, 8, 16, 0, 0};
static const u8 glyph_colon[7] = {0, 12, 12, 0, 12, 12, 0};
static const u8 glyph_semicolon[7] = {0, 12, 12, 0, 12, 12, 8};
static const u8 glyph_quote[7] = {12, 12, 8, 0, 0, 0, 0};
static const u8 glyph_dquote[7] = {10, 10, 0, 0, 0, 0, 0};
static const u8 glyph_lparen[7] = {2, 4, 8, 8, 8, 4, 2};
static const u8 glyph_rparen[7] = {8, 4, 2, 2, 2, 4, 8};
static const u8 glyph_lbracket[7] = {14, 8, 8, 8, 8, 8, 14};
static const u8 glyph_rbracket[7] = {14, 2, 2, 2, 2, 2, 14};
static const u8 glyph_gt[7] = {16, 8, 4, 2, 4, 8, 16};
static const u8 glyph_lt[7] = {1, 2, 4, 8, 4, 2, 1};
static const u8 glyph_eq[7] = {0, 0, 31, 0, 31, 0, 0};
static const u8 glyph_plus[7] = {0, 4, 4, 31, 4, 4, 0};
static const u8 glyph_underscore[7] = {0, 0, 0, 0, 0, 0, 31};

static const u8 glyph_digits[10][7] = {
    {14, 17, 19, 21, 25, 17, 14},
    {4, 12, 4, 4, 4, 4, 14},
    {14, 17, 1, 2, 4, 8, 31},
    {30, 1, 1, 14, 1, 1, 30},
    {2, 6, 10, 18, 31, 2, 2},
    {31, 16, 16, 30, 1, 1, 30},
    {6, 8, 16, 30, 17, 17, 14},
    {31, 1, 2, 4, 8, 8, 8},
    {14, 17, 17, 14, 17, 17, 14},
    {14, 17, 17, 15, 1, 2, 28}
};

static const u8 glyph_upper[26][7] = {
    {14, 17, 17, 31, 17, 17, 17},
    {30, 17, 17, 30, 17, 17, 30},
    {14, 17, 16, 16, 16, 17, 14},
    {30, 17, 17, 17, 17, 17, 30},
    {31, 16, 16, 30, 16, 16, 31},
    {31, 16, 16, 30, 16, 16, 16},
    {14, 17, 16, 16, 19, 17, 15},
    {17, 17, 17, 31, 17, 17, 17},
    {14, 4, 4, 4, 4, 4, 14},
    {1, 1, 1, 1, 17, 17, 14},
    {17, 18, 20, 24, 20, 18, 17},
    {16, 16, 16, 16, 16, 16, 31},
    {17, 27, 21, 21, 17, 17, 17},
    {17, 17, 25, 21, 19, 17, 17},
    {14, 17, 17, 17, 17, 17, 14},
    {30, 17, 17, 30, 16, 16, 16},
    {14, 17, 17, 17, 21, 18, 13},
    {30, 17, 17, 30, 20, 18, 17},
    {15, 16, 16, 14, 1, 1, 30},
    {31, 4, 4, 4, 4, 4, 4},
    {17, 17, 17, 17, 17, 17, 14},
    {17, 17, 17, 17, 17, 10, 4},
    {17, 17, 17, 21, 21, 21, 10},
    {17, 17, 10, 4, 10, 17, 17},
    {17, 17, 10, 4, 4, 4, 4},
    {31, 1, 2, 4, 8, 16, 31}
};

static const u8 *glyph_for_char(char c) {
    if (c >= '0' && c <= '9') {
        return glyph_digits[c - '0'];
    }
    if (c >= 'A' && c <= 'Z') {
        return glyph_upper[c - 'A'];
    }
    if (c >= 'a' && c <= 'z') {
        return glyph_upper[c - 'a'];
    }
    switch (c) {
    case ' ':
        return glyph_space;
    case '-':
        return glyph_dash;
    case '.':
        return glyph_dot;
    case '/':
        return glyph_slash;
    case ':':
        return glyph_colon;
    case ';':
        return glyph_semicolon;
    case '\'':
        return glyph_quote;
    case '"':
        return glyph_dquote;
    case '(':
        return glyph_lparen;
    case ')':
        return glyph_rparen;
    case '[':
        return glyph_lbracket;
    case ']':
        return glyph_rbracket;
    case '>':
        return glyph_gt;
    case '<':
        return glyph_lt;
    case '=':
        return glyph_eq;
    case '+':
        return glyph_plus;
    case '_':
        return glyph_underscore;
    default:
        return glyph_unknown;
    }
}

static u8 rgb_scale(u8 value, u8 bits) {
    if (bits >= 8u) {
        return value;
    }
    return (u8)(value >> (8u - bits));
}

static u32 fb_pack_color(u8 color) {
    u32 rgb = palette[color & 0x0Fu];
    u8 r = (u8)((rgb >> 16) & 0xFFu);
    u8 g = (u8)((rgb >> 8) & 0xFFu);
    u8 b = (u8)(rgb & 0xFFu);
    u32 out = 0;

    out |= (u32)rgb_scale(r, fb.red_mask) << fb.red_pos;
    out |= (u32)rgb_scale(g, fb.green_mask) << fb.green_pos;
    out |= (u32)rgb_scale(b, fb.blue_mask) << fb.blue_pos;
    return out;
}

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

static void put_pixel(u32 x, u32 y, u8 color) {
    u32 packed;
    volatile u8 *pixel;

    if (x >= fb.width || y >= fb.height) {
        return;
    }

    packed = fb_pack_color(color);
    pixel = fb.base + (y * fb.pitch) + (x * (fb.bpp / 8u));

    if (fb.bpp == 32u) {
        pixel[0] = (u8)(packed & 0xFFu);
        pixel[1] = (u8)((packed >> 8) & 0xFFu);
        pixel[2] = (u8)((packed >> 16) & 0xFFu);
        pixel[3] = (u8)((packed >> 24) & 0xFFu);
    } else if (fb.bpp == 24u) {
        pixel[0] = (u8)(packed & 0xFFu);
        pixel[1] = (u8)((packed >> 8) & 0xFFu);
        pixel[2] = (u8)((packed >> 16) & 0xFFu);
    }
}

static void fb_fill_rect(u32 x, u32 y, u32 w, u32 h, u8 color) {
    /* Pack color once outside all loops — palette lookup + shifts are expensive
       on a 486 and the color is constant for the whole rectangle. */
    u32 packed = fb_pack_color(color);
    u32 bytes_pp = fb.bpp / 8u;
    u32 row;
    u32 col;

    for (row = 0; row < h; ++row) {
        u32 py = y + row;
        volatile u8 *row_ptr;

        if (py >= fb.height) {
            break;
        }
        /* Compute the row base pointer once per row instead of per pixel. */
        row_ptr = fb.base + (py * fb.pitch) + (x * bytes_pp);

        for (col = 0; col < w; ++col) {
            volatile u8 *p;

            if (x + col >= fb.width) {
                break;
            }
            p = row_ptr + (col * bytes_pp);
            if (fb.bpp == 32u) {
                p[0] = (u8)(packed & 0xFFu);
                p[1] = (u8)((packed >> 8) & 0xFFu);
                p[2] = (u8)((packed >> 16) & 0xFFu);
                p[3] = (u8)((packed >> 24) & 0xFFu);
            } else if (fb.bpp == 24u) {
                p[0] = (u8)(packed & 0xFFu);
                p[1] = (u8)((packed >> 8) & 0xFFu);
                p[2] = (u8)((packed >> 16) & 0xFFu);
            }
        }
    }
}

static u32 text_index(u16 row, u16 col) {
    return (u32)row * text_cols + col;
}

static void fb_draw_cell(u16 row, u16 col, int cursor_visible) {
    const u8 *glyph;
    u32 i;
    u32 j;
    u32 x = (u32)col * FONT_W;
    u32 y = (u32)row * CELL_H;
    u32 idx = text_index(row, col);
    u8 fg = text_fg[idx];
    u8 bg = text_bg[idx];
    char c = text_chars[idx];

    fb_fill_rect(x, y, FONT_W, CELL_H, bg);
    glyph = glyph_for_char(c);

    for (i = 0; i < 7u; ++i) {
        u8 bits = glyph[i];
        for (j = 0; j < 5u; ++j) {
            if ((bits & (1u << (4u - j))) != 0u) {
                put_pixel(x + 1u + j, y + 4u + i, fg);
            }
        }
    }

    if (cursor_visible) {
        fb_fill_rect(x, y + CELL_H - 2u, FONT_W, 2u, fg);
    }
}

static void fb_redraw_all(void) {
    u16 row;
    u16 col;
    for (row = 0; row < text_rows; ++row) {
        for (col = 0; col < text_cols; ++col) {
            fb_draw_cell(row, col, row == cursor_row && col == cursor_col);
        }
    }
}

static void fb_set_cursor(u16 new_row, u16 new_col) {
    if (cursor_row < text_rows && cursor_col < text_cols) {
        fb_draw_cell(cursor_row, cursor_col, 0);
    }
    cursor_row = new_row;
    cursor_col = new_col;
    if (cursor_row < text_rows && cursor_col < text_cols) {
        fb_draw_cell(cursor_row, cursor_col, 1);
    }
}

static u8 to_vga_attr(u8 fg, u8 bg) {
    return (u8)(((bg & 0x0Fu) << 4) | (fg & 0x0Fu));
}

void term_init(void) {
    backend = DISPLAY_VGA_TEXT;
    text_cols = VGA_COLS;
    text_rows = VGA_ROWS;
    cursor_row = 0;
    cursor_col = 0;
    current_fg = 7;
    current_bg = 0;
    term_enable_hw_cursor(14, 15);
    term_update_hw_cursor();
}

void term_init_with_multiboot(const struct multiboot_info *mbi, u32 magic) {
    term_init();

    if (magic != MULTIBOOT_MAGIC || !mbi || (mbi->flags & (1u << 12)) == 0u) {
        return;
    }
    if (mbi->framebuffer_type != 1u) {
        return;
    }
    if (mbi->framebuffer_bpp != 24u && mbi->framebuffer_bpp != 32u) {
        return;
    }

    fb.base = (volatile u8 *)(u32)mbi->framebuffer_addr;
    fb.pitch = mbi->framebuffer_pitch;
    fb.width = mbi->framebuffer_width;
    fb.height = mbi->framebuffer_height;
    fb.bpp = mbi->framebuffer_bpp;
    fb.type = mbi->framebuffer_type;
    fb.red_pos = mbi->framebuffer_color_info[0];
    fb.red_mask = mbi->framebuffer_color_info[1];
    fb.green_pos = mbi->framebuffer_color_info[2];
    fb.green_mask = mbi->framebuffer_color_info[3];
    fb.blue_pos = mbi->framebuffer_color_info[4];
    fb.blue_mask = mbi->framebuffer_color_info[5];

    if (fb.width < FONT_W || fb.height < CELL_H) {
        return;
    }

    text_cols = (u16)(fb.width / FONT_W);
    text_rows = (u16)(fb.height / CELL_H);
    if (text_cols > MAX_CONSOLE_COLS) {
        text_cols = MAX_CONSOLE_COLS;
    }
    if (text_rows > MAX_CONSOLE_ROWS) {
        text_rows = MAX_CONSOLE_ROWS;
    }

    backend = DISPLAY_FRAMEBUFFER;
    cursor_row = 0;
    cursor_col = 0;
}

void term_set_color(u8 fg, u8 bg) {
    current_fg = (u8)(fg & 0x0Fu);
    current_bg = (u8)(bg & 0x0Fu);
}

void term_get_color(u8 *fg, u8 *bg) {
    if (fg) {
        *fg = current_fg;
    }
    if (bg) {
        *bg = current_bg;
    }
}

int console_has_framebuffer(void) {
    return backend == DISPLAY_FRAMEBUFFER;
}

u32 console_width(void) {
    return backend == DISPLAY_FRAMEBUFFER ? fb.width : VGA_COLS;
}

u32 console_height(void) {
    return backend == DISPLAY_FRAMEBUFFER ? fb.height : VGA_ROWS;
}

void term_clear(void) {
    u32 i;
    for (i = 0; i < (u32)text_cols * text_rows; ++i) {
        text_chars[i] = ' ';
        text_fg[i] = current_fg;
        text_bg[i] = current_bg;
    }

    if (backend == DISPLAY_FRAMEBUFFER) {
        fb_fill_rect(0, 0, fb.width, fb.height, current_bg);
    } else {
        for (i = 0; i < VGA_COLS * VGA_ROWS; ++i) {
            vga[i] = ((u16)to_vga_attr(current_fg, current_bg) << 8) | ' ';
        }
    }

    cursor_row = 0;
    cursor_col = 0;
    if (backend == DISPLAY_FRAMEBUFFER) {
        fb_redraw_all();
    } else {
        term_update_hw_cursor();
    }
}

static void term_scroll_if_needed(void) {
    u16 row;
    u16 col;
    if (cursor_row < text_rows) {
        return;
    }

    for (row = 1; row < text_rows; ++row) {
        for (col = 0; col < text_cols; ++col) {
            u32 src = text_index(row, col);
            u32 dst = text_index((u16)(row - 1u), col);
            text_chars[dst] = text_chars[src];
            text_fg[dst] = text_fg[src];
            text_bg[dst] = text_bg[src];
        }
    }

    for (col = 0; col < text_cols; ++col) {
        u32 idx = text_index((u16)(text_rows - 1u), col);
        text_chars[idx] = ' ';
        text_fg[idx] = current_fg;
        text_bg[idx] = current_bg;
    }

    cursor_row = (u16)(text_rows - 1u);
    if (backend == DISPLAY_FRAMEBUFFER) {
        fb_redraw_all();
    } else {
        for (row = 0; row < text_rows; ++row) {
            for (col = 0; col < text_cols; ++col) {
                u32 idx = text_index(row, col);
                vga[row * VGA_COLS + col] =
                    ((u16)to_vga_attr(text_fg[idx], text_bg[idx]) << 8) | (u8)text_chars[idx];
            }
        }
    }
}

static void write_local_char(char c) {
    u32 idx;

    if (c == '\n') {
        /* Erase cursor at the old position before moving it. The previous code
           called fb_set_cursor(same, same) here which erased then immediately
           re-drew at the same cell — net visual effect: none — and then the
           second fb_set_cursor erased the *new* position instead, leaving a
           ghost cursor at the old cell whenever no scroll occurred. */
        if (backend == DISPLAY_FRAMEBUFFER) {
            if (cursor_row < text_rows && cursor_col < text_cols) {
                fb_draw_cell(cursor_row, cursor_col, 0);
            }
        }
        cursor_col = 0;
        ++cursor_row;
        term_scroll_if_needed();
        if (backend == DISPLAY_FRAMEBUFFER) {
            if (cursor_row < text_rows && cursor_col < text_cols) {
                fb_draw_cell(cursor_row, cursor_col, 1);
            }
        } else {
            term_update_hw_cursor();
        }
        return;
    }

    if (c == '\r') {
        if (backend == DISPLAY_FRAMEBUFFER) {
            if (cursor_row < text_rows && cursor_col < text_cols) {
                fb_draw_cell(cursor_row, cursor_col, 0);
            }
        }
        cursor_col = 0;
        if (backend == DISPLAY_FRAMEBUFFER) {
            if (cursor_row < text_rows && cursor_col < text_cols) {
                fb_draw_cell(cursor_row, cursor_col, 1);
            }
        } else {
            term_update_hw_cursor();
        }
        return;
    }

    if (c == '\b') {
        if (cursor_col > 0) {
            --cursor_col;
            idx = text_index(cursor_row, cursor_col);
            text_chars[idx] = ' ';
            text_fg[idx] = current_fg;
            text_bg[idx] = current_bg;
            if (backend == DISPLAY_FRAMEBUFFER) {
                fb_draw_cell(cursor_row, cursor_col, 1);
            } else {
                vga[cursor_row * VGA_COLS + cursor_col] =
                    ((u16)to_vga_attr(current_fg, current_bg) << 8) | ' ';
                term_update_hw_cursor();
            }
        }
        return;
    }

    idx = text_index(cursor_row, cursor_col);
    text_chars[idx] = c;
    text_fg[idx] = current_fg;
    text_bg[idx] = current_bg;

    if (backend == DISPLAY_FRAMEBUFFER) {
        fb_draw_cell(cursor_row, cursor_col, 0);
    } else {
        vga[cursor_row * VGA_COLS + cursor_col] =
            ((u16)to_vga_attr(current_fg, current_bg) << 8) | (u8)c;
    }

    ++cursor_col;
    if (cursor_col >= text_cols) {
        cursor_col = 0;
        ++cursor_row;
        term_scroll_if_needed();
    }

    if (backend == DISPLAY_FRAMEBUFFER) {
        fb_set_cursor(cursor_row, cursor_col);
    } else {
        term_update_hw_cursor();
    }
}

void console_putchar(enum console_target target, char c) {
    if ((target & CONSOLE_VGA) != 0) {
        write_local_char(c);
    }
    if ((target & CONSOLE_SERIAL) != 0 && serial_is_ready()) {
        serial_putchar(c);
    }
}

void term_putchar(char c) {
    console_putchar(CONSOLE_BOTH, c);
}

void console_print(enum console_target target, const char *s) {
    while (*s) {
        console_putchar(target, *s++);
    }
}

void term_print(const char *s) {
    console_print(CONSOLE_BOTH, s);
}

void console_print_u32_dec(enum console_target target, u32 value) {
    char buf[11];
    int i = 0;

    if (value == 0u) {
        console_putchar(target, '0');
        return;
    }

    while (value > 0u && i < 10) {
        buf[i++] = (char)('0' + (value % 10u));
        value /= 10u;
    }

    while (i > 0) {
        console_putchar(target, buf[--i]);
    }
}

void term_print_u32_dec(u32 value) {
    console_print_u32_dec(CONSOLE_BOTH, value);
}

static void console_print_hex_nibble(enum console_target target, u8 value) {
    value &= 0x0Fu;
    if (value < 10u) {
        console_putchar(target, (char)('0' + value));
    } else {
        console_putchar(target, (char)('A' + (value - 10u)));
    }
}

void console_print_hex_u32(enum console_target target, u32 value) {
    int shift;
    console_print(target, "0x");
    for (shift = 28; shift >= 0; shift -= 4) {
        console_print_hex_nibble(target, (u8)((value >> shift) & 0x0Fu));
    }
}

void term_print_hex_u32(u32 value) {
    console_print_hex_u32(CONSOLE_BOTH, value);
}

void console_vprintf(enum console_target target, const char *fmt, va_list args) {
    while (*fmt) {
        if (*fmt != '%') {
            console_putchar(target, *fmt++);
            continue;
        }

        ++fmt;
        if (*fmt == '\0') {
            break;
        }

        if (*fmt == '%') {
            console_putchar(target, '%');
        } else if (*fmt == 'c') {
            console_putchar(target, (char)va_arg(args, int));
        } else if (*fmt == 's') {
            const char *s = va_arg(args, const char *);
            console_print(target, s ? s : "(null)");
        } else if (*fmt == 'u') {
            console_print_u32_dec(target, va_arg(args, u32));
        } else if (*fmt == 'x') {
            console_print_hex_u32(target, va_arg(args, u32));
        } else {
            console_putchar(target, '%');
            console_putchar(target, *fmt);
        }

        ++fmt;
    }
}

void term_vprintf(const char *fmt, va_list args) {
    console_vprintf(CONSOLE_BOTH, fmt, args);
}

void console_printf(enum console_target target, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    console_vprintf(target, fmt, args);
    va_end(args);
}

void term_printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    console_vprintf(CONSOLE_BOTH, fmt, args);
    va_end(args);
}
