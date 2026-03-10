#include "kernel.h"

struct shell_context {
    enum console_target output;
    char input[64];
    u32 len;
    struct vfs_node *cwd;
};

static void shell_print_help(struct shell_context *ctx) {
    console_print(ctx->output, "Commands:\n");
    console_print(ctx->output, "  help       - list commands\n");
    console_print(ctx->output, "  clear      - clear screen\n");
    console_print(ctx->output, "  systeminfo - detected CPU/memory info\n");
    console_print(ctx->output, "  about      - alias for systeminfo\n");
    console_print(ctx->output, "  heap       - show kernel heap usage\n");
    console_print(ctx->output, "  alloc N    - allocate N bytes from the kernel heap\n");
    console_print(ctx->output, "  ls [PATH]  - list directory contents\n");
    console_print(ctx->output, "  cd PATH    - change directory\n");
    console_print(ctx->output, "  pwd        - print current directory\n");
    console_print(ctx->output, "  mkdir PATH - create a directory\n");
    console_print(ctx->output, "  touch PATH - create an empty file\n");
    console_print(ctx->output, "  cat PATH   - print a file\n");
    console_print(ctx->output, "  echo TEXT  - print TEXT\n");
    console_print(ctx->output, "  echo TEXT > PATH - write TEXT to a file\n");
    console_print(ctx->output, "  reboot     - reset machine\n");
    console_print(ctx->output, "  halt       - stop CPU\n");
}

static void shell_print_heap_summary(struct shell_context *ctx) {
    if (!heap_is_ready()) {
        console_print(ctx->output, "Heap is unavailable. Multiboot memory info was not provided.\n");
        return;
    }

    console_print(ctx->output, "Heap usage: ");
    console_print_u32_dec(ctx->output, heap_bytes_used());
    console_print(ctx->output, " / ");
    console_print_u32_dec(ctx->output, heap_bytes_total());
    console_print(ctx->output, " bytes, free ");
    console_print_u32_dec(ctx->output, heap_bytes_free());
    console_print(ctx->output, "\n");
}

static void shell_handle_alloc(struct shell_context *ctx, const char *arg) {
    int ok = 0;
    u32 size = parse_u32_dec(arg, &ok);
    void *ptr;

    if (!ok || size == 0u) {
        console_print(ctx->output, "Usage: alloc N\n");
        return;
    }

    ptr = kmalloc(size);
    if (!ptr) {
        console_print(ctx->output, "Allocation failed.\n");
        return;
    }

    console_print(ctx->output, "Allocated ");
    console_print_u32_dec(ctx->output, size);
    console_print(ctx->output, " bytes at ");
    console_print_hex_u32(ctx->output, (u32)ptr);
    console_print(ctx->output, "\n");
}

static const char *skip_spaces(const char *s) {
    while (*s == ' ') {
        ++s;
    }
    return s;
}

static char *find_redirect(char *s) {
    while (*s) {
        if (*s == '>') {
            return s;
        }
        ++s;
    }
    return (char *)0;
}

static void shell_handle_pwd(struct shell_context *ctx) {
    char path[128];
    vfs_get_cwd_path(ctx->cwd, path, sizeof(path));
    console_printf(ctx->output, "%s\n", path);
}

static void shell_handle_ls(struct shell_context *ctx, const char *arg) {
    arg = skip_spaces(arg);
    if (*arg == '\0') {
        vfs_list(ctx->cwd, (const char *)0, ctx->output);
        return;
    }
    vfs_list(ctx->cwd, arg, ctx->output);
}

static void shell_handle_cd(struct shell_context *ctx, const char *arg) {
    arg = skip_spaces(arg);
    if (*arg == '\0') {
        console_print(ctx->output, "Usage: cd PATH\n");
        return;
    }
    if (!vfs_change_dir(&ctx->cwd, arg)) {
        console_print(ctx->output, "cd: no such directory\n");
    }
}

static void shell_handle_mkdir(struct shell_context *ctx, const char *arg) {
    arg = skip_spaces(arg);
    if (*arg == '\0') {
        console_print(ctx->output, "Usage: mkdir PATH\n");
        return;
    }
    if (!vfs_make_dir(ctx->cwd, arg)) {
        console_print(ctx->output, "mkdir: failed\n");
    }
}

static void shell_handle_touch(struct shell_context *ctx, const char *arg) {
    arg = skip_spaces(arg);
    if (*arg == '\0') {
        console_print(ctx->output, "Usage: touch PATH\n");
        return;
    }
    if (!vfs_touch(ctx->cwd, arg)) {
        console_print(ctx->output, "touch: failed\n");
    }
}

static void shell_handle_cat(struct shell_context *ctx, const char *arg) {
    const char *data;
    u32 size;

    arg = skip_spaces(arg);
    if (*arg == '\0') {
        console_print(ctx->output, "Usage: cat PATH\n");
        return;
    }
    if (!vfs_read_file(ctx->cwd, arg, &data, &size)) {
        console_print(ctx->output, "cat: no such file\n");
        return;
    }
    if (size > 0u && data) {
        console_print(ctx->output, data);
    }
    console_putchar(ctx->output, '\n');
}

static void shell_handle_echo(struct shell_context *ctx, char *arg) {
    char *redirect;

    arg = (char *)skip_spaces(arg);
    redirect = find_redirect(arg);
    if (!redirect) {
        console_print(ctx->output, arg);
        console_putchar(ctx->output, '\n');
        return;
    }

    *redirect = '\0';
    ++redirect;
    redirect = (char *)skip_spaces(redirect);

    {
        u32 len = str_len(arg);
        while (len > 0u && arg[len - 1u] == ' ') {
            arg[len - 1u] = '\0';
            --len;
        }

        if (*redirect == '\0') {
            console_print(ctx->output, "echo: missing redirect path\n");
            return;
        }

        if (!vfs_write_file(ctx->cwd, redirect, arg, len)) {
            console_print(ctx->output, "echo: write failed\n");
        }
    }
}

static void shell_print_prompt(struct shell_context *ctx) {
    char path[128];
    u8 fg;
    u8 bg;

    vfs_get_cwd_path(ctx->cwd, path, sizeof(path));
    term_get_color(&fg, &bg);
    term_set_color(10, 0);
    console_printf(ctx->output, "%s> ", path);
    term_set_color(fg, bg);
}

static int shell_try_get_input(enum console_target target, char *out) {
    if (target == CONSOLE_SERIAL) {
        return serial_try_read_char(out);
    }
    return keyboard_try_read_char(out);
}

static void shell_process_char(struct shell_context *ctx, const struct multiboot_info *mbi, u32 magic, char c) {
    if (c == '\r') {
        c = '\n';
    }

    if (c == '\n') {
        ctx->input[ctx->len] = '\0';
        console_putchar(ctx->output, '\n');

        if (ctx->len == 0u) {
            shell_print_prompt(ctx);
            return;
        }

        if (str_eq(ctx->input, "help")) {
            shell_print_help(ctx);
        } else if (str_eq(ctx->input, "clear")) {
            if (ctx->output == CONSOLE_VGA) {
                term_clear();
            } else {
                console_print(ctx->output, "\n");
            }
        } else if (str_eq(ctx->input, "systeminfo") || str_eq(ctx->input, "about")) {
            print_systeminfo(mbi, magic);
        } else if (str_eq(ctx->input, "heap")) {
            shell_print_heap_summary(ctx);
        } else if (str_startswith(ctx->input, "alloc ")) {
            shell_handle_alloc(ctx, ctx->input + 6);
        } else if (str_eq(ctx->input, "pwd")) {
            shell_handle_pwd(ctx);
        } else if (str_eq(ctx->input, "ls")) {
            shell_handle_ls(ctx, "");
        } else if (str_startswith(ctx->input, "ls ")) {
            shell_handle_ls(ctx, ctx->input + 3);
        } else if (str_startswith(ctx->input, "cd ")) {
            shell_handle_cd(ctx, ctx->input + 3);
        } else if (str_startswith(ctx->input, "mkdir ")) {
            shell_handle_mkdir(ctx, ctx->input + 6);
        } else if (str_startswith(ctx->input, "touch ")) {
            shell_handle_touch(ctx, ctx->input + 6);
        } else if (str_startswith(ctx->input, "cat ")) {
            shell_handle_cat(ctx, ctx->input + 4);
        } else if (str_startswith(ctx->input, "echo ")) {
            shell_handle_echo(ctx, ctx->input + 5);
        } else if (str_eq(ctx->input, "reboot")) {
            term_print("Rebooting...\n");
            try_reboot();
        } else if (str_eq(ctx->input, "halt")) {
            term_print("CPU halted. Reset VM to continue.\n");
            halt_forever();
        } else {
            console_print(ctx->output, "Unknown command. Type \"help\".\n");
        }

        ctx->len = 0;
        shell_print_prompt(ctx);
        return;
    }

    if (c == '\b' || c == 127) {
        if (ctx->len > 0u) {
            --ctx->len;
            console_putchar(ctx->output, '\b');
        }
        return;
    }

    if (c >= ' ' && c <= '~' && ctx->len < (sizeof(ctx->input) - 1u)) {
        ctx->input[ctx->len++] = c;
        console_putchar(ctx->output, c);
    }
}

void shell_loop(const struct multiboot_info *mbi, u32 magic) {
    struct shell_context vga_shell;
    struct shell_context serial_shell;
    char c;

    vga_shell.output = CONSOLE_VGA;
    vga_shell.len = 0;
    vga_shell.cwd = vfs_root();

    serial_shell.output = CONSOLE_SERIAL;
    serial_shell.len = 0;
    serial_shell.cwd = vfs_root();

    term_set_color(11, 0);
    console_print(CONSOLE_VGA, "SLOP OS (microkernel bootstrap)\n");
    term_set_color(7, 0);
    console_print(CONSOLE_VGA, "Type \"help\" for commands.\n\n");
    shell_print_prompt(&vga_shell);

    if (serial_is_ready()) {
        term_set_color(11, 0);
        console_print(CONSOLE_SERIAL, "SLOP serial console\n");
        term_set_color(7, 0);
        console_print(CONSOLE_SERIAL, "Type \"help\" for commands.\n\n");
        shell_print_prompt(&serial_shell);
    }

    for (;;) {
        if (shell_try_get_input(CONSOLE_VGA, &c)) {
            shell_process_char(&vga_shell, mbi, magic, c);
        }
        if (serial_is_ready() && shell_try_get_input(CONSOLE_SERIAL, &c)) {
            shell_process_char(&serial_shell, mbi, magic, c);
        }
        cpu_relax_wait();
    }
}
