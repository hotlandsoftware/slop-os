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
};

extern char _kernel_start;
extern char _kernel_end;

void term_init(void);
void term_clear(void);
void term_putchar(char c);
void term_print(const char *s);
void term_print_u32_dec(u32 value);
void term_print_hex_u32(u32 value);
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
int vfs_change_dir(const char *path);
int vfs_make_dir(const char *path);
int vfs_touch(const char *path);
void vfs_list(const char *path);
int vfs_read_file(const char *path, const char **data, u32 *size);
void vfs_get_cwd_path(char *buf, size_t size);

void print_systeminfo(const struct multiboot_info *mbi, u32 magic);
void shell_loop(const struct multiboot_info *mbi, u32 magic);

#endif
