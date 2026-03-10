#include "kernel.h"

static void print_help(void) {
    term_print("Commands:\n");
    term_print("  help       - list commands\n");
    term_print("  clear      - clear screen\n");
    term_print("  systeminfo - detected CPU/memory info\n");
    term_print("  about      - alias for systeminfo\n");
    term_print("  heap       - show kernel heap usage\n");
    term_print("  alloc N    - allocate N bytes from the kernel heap\n");
    term_print("  ls [PATH]  - list directory contents\n");
    term_print("  cd PATH    - change directory\n");
    term_print("  pwd        - print current directory\n");
    term_print("  mkdir PATH - create a directory\n");
    term_print("  touch PATH - create an empty file\n");
    term_print("  cat PATH   - print a file\n");
    term_print("  echo TEXT  - print TEXT\n");
    term_print("  reboot     - reset machine\n");
    term_print("  halt       - stop CPU\n");
}

static void print_heap_summary(void) {
    if (!heap_is_ready()) {
        term_print("Heap is unavailable. Multiboot memory info was not provided.\n");
        return;
    }

    term_print("Heap usage: ");
    term_print_u32_dec(heap_bytes_used());
    term_print(" / ");
    term_print_u32_dec(heap_bytes_total());
    term_print(" bytes, free ");
    term_print_u32_dec(heap_bytes_free());
    term_print("\n");
}

static void handle_alloc(const char *arg) {
    int ok = 0;
    u32 size = parse_u32_dec(arg, &ok);
    void *ptr;

    if (!ok || size == 0u) {
        term_print("Usage: alloc N\n");
        return;
    }

    ptr = kmalloc(size);
    if (!ptr) {
        term_print("Allocation failed.\n");
        return;
    }

    term_print("Allocated ");
    term_print_u32_dec(size);
    term_print(" bytes at ");
    term_print_hex_u32((u32)ptr);
    term_print("\n");
}

static const char *skip_spaces(const char *s) {
    while (*s == ' ') {
        ++s;
    }
    return s;
}

static void handle_pwd(void) {
    char path[128];
    vfs_get_cwd_path(path, sizeof(path));
    term_printf("%s\n", path);
}

static void handle_ls(const char *arg) {
    arg = skip_spaces(arg);
    if (*arg == '\0') {
        vfs_list((const char *)0);
        return;
    }
    vfs_list(arg);
}

static void handle_cd(const char *arg) {
    arg = skip_spaces(arg);
    if (*arg == '\0') {
        term_print("Usage: cd PATH\n");
        return;
    }
    if (!vfs_change_dir(arg)) {
        term_print("cd: no such directory\n");
    }
}

static void handle_mkdir(const char *arg) {
    arg = skip_spaces(arg);
    if (*arg == '\0') {
        term_print("Usage: mkdir PATH\n");
        return;
    }
    if (!vfs_make_dir(arg)) {
        term_print("mkdir: failed\n");
    }
}

static void handle_touch(const char *arg) {
    arg = skip_spaces(arg);
    if (*arg == '\0') {
        term_print("Usage: touch PATH\n");
        return;
    }
    if (!vfs_touch(arg)) {
        term_print("touch: failed\n");
    }
}

static void handle_cat(const char *arg) {
    const char *data;
    u32 size;

    arg = skip_spaces(arg);
    if (*arg == '\0') {
        term_print("Usage: cat PATH\n");
        return;
    }
    if (!vfs_read_file(arg, &data, &size)) {
        term_print("cat: no such file\n");
        return;
    }
    if (size > 0u && data) {
        term_print(data);
    }
    term_putchar('\n');
}

static void print_prompt(void) {
    char path[128];
    vfs_get_cwd_path(path, sizeof(path));
    term_printf("%s> ", path);
}

void shell_loop(const struct multiboot_info *mbi, u32 magic) {
    char input[64];
    u32 len = 0;

    term_print("SLOP OS (microkernel bootstrap)\n");
    term_print("Type \"help\" for commands.\n\n");
    print_prompt();

    for (;;) {
        char c;

        if (!keyboard_try_read_char(&c)) {
            cpu_relax_wait();
            continue;
        }

        if (c == '\n') {
            input[len] = '\0';
            term_putchar('\n');

            if (len == 0u) {
                print_prompt();
                continue;
            }

            if (str_eq(input, "help")) {
                print_help();
            } else if (str_eq(input, "clear")) {
                term_clear();
            } else if (str_eq(input, "systeminfo") || str_eq(input, "about")) {
                print_systeminfo(mbi, magic);
            } else if (str_eq(input, "heap")) {
                print_heap_summary();
            } else if (str_startswith(input, "alloc ")) {
                handle_alloc(input + 6);
            } else if (str_eq(input, "pwd")) {
                handle_pwd();
            } else if (str_eq(input, "ls")) {
                handle_ls("");
            } else if (str_startswith(input, "ls ")) {
                handle_ls(input + 3);
            } else if (str_startswith(input, "cd ")) {
                handle_cd(input + 3);
            } else if (str_startswith(input, "mkdir ")) {
                handle_mkdir(input + 6);
            } else if (str_startswith(input, "touch ")) {
                handle_touch(input + 6);
            } else if (str_startswith(input, "cat ")) {
                handle_cat(input + 4);
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
            print_prompt();
            continue;
        }

        if (c == '\b') {
            if (len > 0u) {
                --len;
                term_putchar('\b');
            }
            continue;
        }

        if (c >= ' ' && c <= '~' && len < (sizeof(input) - 1u)) {
            input[len++] = c;
            term_putchar(c);
        }
    }
}
