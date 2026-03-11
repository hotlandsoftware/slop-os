#include "kernel.h"

void kmain(u32 magic, u32 mbi_addr) {
    const struct multiboot_info *mbi = (const struct multiboot_info *)mbi_addr;

    term_init_with_multiboot(mbi, magic);
    serial_init();
    term_clear();
    term_print("kmain: console ok\n");
    protection_init();
    term_print("kmain: protection ok\n");
    heap_init(mbi, magic);
    term_print("kmain: heap ok\n");
    proc_init();
    term_print("kmain: proc ok\n");
    tasking_init();
    task_spawn_kernel("shell-vga", 0);
    task_spawn_kernel("shell-serial", 0);
    term_print("kmain: tasking ok\n");
    interrupts_init();
    enable_interrupts();
    term_print("kmain: irq timer on\n");
    storage_init();
    term_print("kmain: storage ok\n");
    fs_init();
    term_print("kmain: fs ok\n");
    vfs_init();
    term_print("kmain: vfs ok\n");
    exec_seed_programs();
    term_print("kmain: exec stubs ok\n");
    term_print("kmain: entering shell\n");
    shell_loop(mbi, magic);
}
