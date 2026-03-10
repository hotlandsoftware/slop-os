#include "kernel.h"

void kmain(u32 magic, u32 mbi_addr) {
    const struct multiboot_info *mbi = (const struct multiboot_info *)mbi_addr;

    term_init();
    term_clear();
    term_print("kmain: console ok\n");
    heap_init(mbi, magic);
    term_print("kmain: heap ok\n");
    vfs_init();
    term_print("kmain: vfs ok\n");
    term_print("kmain: entering shell\n");
    shell_loop(mbi, magic);
}
