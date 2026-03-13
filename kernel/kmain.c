#include "kernel.h"

void kmain(u32 magic, u32 mbi_addr) {
    const struct multiboot_info *mbi = (const struct multiboot_info *)mbi_addr;
    const struct block_device *cd0;
    int imported_programs;

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
    ipc_init();
    term_print("kmain: ipc ok\n");
    service_init();
    term_print("kmain: services ok\n");
    tasking_init();
    task_spawn_kernel("shell-vga", 0);
    task_spawn_kernel("shell-serial", 0);
    term_print("kmain: tasking ok\n");
    interrupts_init();
    enable_interrupts();
    term_print("kmain: irq timer on\n");
    storage_init();
    term_print("kmain: storage ok\n");
    cd0 = storage_find_device("cd0");
    term_print("kmain: cd0 ");
    term_print(cd0 ? "detected\n" : "missing\n");
    fs_init();
    term_print("kmain: fs ok\n");
    term_print("kmain: mounts=");
    term_print_u32_dec(fs_mount_count());
    term_print("\n");
    vfs_init();
    term_print("kmain: vfs ok\n");
    imported_programs = exec_seed_programs();
    term_print("kmain: imported /bin programs=");
    term_print_u32_dec((u32)((imported_programs < 0) ? 0 : imported_programs));
    term_print("\n");
    term_print("kmain: exec stubs ok\n");
    term_print("kmain: entering shell\n");
    shell_loop(mbi, magic);
}
