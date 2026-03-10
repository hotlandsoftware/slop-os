#ifndef SLOP_KERNEL_H
#define SLOP_KERNEL_H

#include <stdarg.h>

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned int size_t;

#define MULTIBOOT_MAGIC 0x2BADB002u

struct multiboot_info {
    u32 flags;
    u32 mem_lower;
    u32 mem_upper;
    u32 boot_device;
    u32 cmdline;
    u32 mods_count;
    u32 mods_addr;
    u32 syms[4];
    u32 mmap_length;
    u32 mmap_addr;
    u32 drives_length;
    u32 drives_addr;
    u32 config_table;
    u32 boot_loader_name;
    u32 apm_table;
    u32 vbe_control_info;
    u32 vbe_mode_info;
    u16 vbe_mode;
    u16 vbe_interface_seg;
    u16 vbe_interface_off;
    u16 vbe_interface_len;
    unsigned long long framebuffer_addr;
    u32 framebuffer_pitch;
    u32 framebuffer_width;
    u32 framebuffer_height;
    u8 framebuffer_bpp;
    u8 framebuffer_type;
    u8 framebuffer_color_info[6];
} __attribute__((packed));

extern char _kernel_start;
extern char _kernel_end;

enum console_target {
    CONSOLE_VGA = 1,
    CONSOLE_SERIAL = 2,
    CONSOLE_BOTH = 3
};

struct vfs_node;

void term_init(void);
void term_clear(void);
void term_putchar(char c);
void term_print(const char *s);
void term_print_u32_dec(u32 value);
void term_print_hex_u32(u32 value);
void console_putchar(enum console_target target, char c);
void console_print(enum console_target target, const char *s);
void console_print_u32_dec(enum console_target target, u32 value);
void console_print_hex_u32(enum console_target target, u32 value);
void console_vprintf(enum console_target target, const char *fmt, va_list args);
void console_printf(enum console_target target, const char *fmt, ...);
void term_init_with_multiboot(const struct multiboot_info *mbi, u32 magic);
void term_set_color(u8 fg, u8 bg);
void term_get_color(u8 *fg, u8 *bg);
int console_has_framebuffer(void);
u32 console_width(void);
u32 console_height(void);
void term_vprintf(const char *fmt, va_list args);
void term_printf(const char *fmt, ...);

int str_eq(const char *a, const char *b);
int str_cmp(const char *a, const char *b);
int str_startswith(const char *s, const char *prefix);
u32 str_len(const char *s);
void str_copy(char *dst, const char *src, size_t size);
u32 parse_u32_dec(const char *s, int *ok);
void mem_zero(void *dst, size_t size);

u8 inb(u16 port);
void outb(u16 port, u8 value);
void halt_forever(void);
void try_reboot(void);
void enable_interrupts(void);
void disable_interrupts(void);
void cpu_idle(void);
int cpu_has_cpuid(void);
void cpu_vendor(char out[13]);
int keyboard_try_read_char(char *out);
int keyboard_read_char_blocking(char *out);
void cpu_relax_wait(void);

void serial_init(void);
int serial_is_ready(void);
void serial_putchar(char c);
void serial_print(const char *s);
int serial_try_read_char(char *out);

void interrupts_init(void);
u32 timer_ticks(void);

void heap_init(const struct multiboot_info *mbi, u32 magic);
void *kmalloc(size_t size);
void *kcalloc(size_t size);
u32 heap_bytes_total(void);
u32 heap_bytes_used(void);
u32 heap_bytes_free(void);
int heap_is_ready(void);

void vfs_init(void);
struct vfs_node *vfs_root(void);
int vfs_change_dir(struct vfs_node **cwd, const char *path);
int vfs_make_dir(struct vfs_node *cwd, const char *path);
int vfs_touch(struct vfs_node *cwd, const char *path);
void vfs_list(struct vfs_node *cwd, const char *path, enum console_target target);
int vfs_read_file(struct vfs_node *cwd, const char *path, const char **data, u32 *size);
int vfs_write_file(struct vfs_node *cwd, const char *path, const char *data, u32 size);
void vfs_get_cwd_path(struct vfs_node *cwd, char *buf, size_t size);

void print_systeminfo(const struct multiboot_info *mbi, u32 magic);
void shell_loop(const struct multiboot_info *mbi, u32 magic);

#endif
