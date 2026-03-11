#include "kernel.h"

#define SHELL_INPUT_MAX 127
#define SHELL_ARG_MAX 16

struct shell_context {
    enum console_target output;
    char input[SHELL_INPUT_MAX + 1];
    u32 len;
    struct vfs_node *cwd;
};

static const char *skip_spaces(const char *s) {
    while (*s == ' ') {
        ++s;
    }
    return s;
}

static int shell_split_args(char *line, char **argv, int max_args) {
    int argc = 0;
    char *p = line;

    while (*p && argc < max_args) {
        p = (char *)skip_spaces(p);
        if (*p == '\0') {
            break;
        }

        if (*p == '>') {
            argv[argc++] = (char *)">";
            ++p;
            continue;
        }

        argv[argc++] = p;
        {
            int split_on_redirect = 0;
            while (*p && *p != ' ') {
                if (*p == '>') {
                    *p = '\0';
                    split_on_redirect = 1;
                    break;
                }
                ++p;
            }
            if (split_on_redirect) {
                continue;
            }
        }
        if (*p == '\0') {
            continue;
        }
        *p++ = '\0';
    }

    return argc;
}

static void shell_print_help(struct shell_context *ctx) {
    console_print(ctx->output, "Commands:\n");
    console_print(ctx->output, "  help       - list commands\n");
    console_print(ctx->output, "  clear      - clear screen\n");
    console_print(ctx->output, "  cd PATH    - change directory (shell builtin)\n");
    console_print(ctx->output, "  alloc N    - allocate N bytes from kernel heap\n");
    console_print(ctx->output, "  reboot     - reset machine\n");
    console_print(ctx->output, "  halt       - stop CPU\n");
    console_print(ctx->output, "Program-style commands (/bin/*):\n");
    console_print(ctx->output, "  ls [PATH], pwd, mkdir PATH, touch PATH\n");
    console_print(ctx->output, "  cat PATH, echo TEXT, echo TEXT > PATH\n");
    console_print(ctx->output, "  systeminfo, mounts, ps, run PATH, ring3test\n");
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

static int shell_run_program(struct shell_context *ctx, const struct multiboot_info *mbi, u32 magic, char *line) {
    char *argv[SHELL_ARG_MAX];
    char path[64];
    struct exec_context exec_ctx;
    int argc = shell_split_args(line, argv, SHELL_ARG_MAX);

    if (argc == 0) {
        return 1;
    }

    if (str_eq(argv[0], "about")) {
        argv[0] = "systeminfo";
    }

    if (argv[0][0] == '/') {
        str_copy(path, argv[0], sizeof(path));
    } else {
        str_copy(path, "/bin/", sizeof(path));
        {
            u32 base = str_len(path);
            u32 i;
            for (i = 0; argv[0][i] != '\0' && (base + i + 1u) < sizeof(path); ++i) {
                path[base + i] = argv[0][i];
                path[base + i + 1u] = '\0';
            }
        }
    }

    exec_ctx.output = ctx->output;
    exec_ctx.cwd = &ctx->cwd;
    exec_ctx.mbi = mbi;
    exec_ctx.magic = magic;
    return exec_run_path(path, argc, argv, &exec_ctx);
}

static void shell_execute_line(struct shell_context *ctx, const struct multiboot_info *mbi, u32 magic, char *line) {
    char *trimmed = (char *)skip_spaces(line);

    if (*trimmed == '\0') {
        return;
    }

    if (str_eq(trimmed, "help")) {
        shell_print_help(ctx);
    } else if (str_eq(trimmed, "clear")) {
        if (ctx->output == CONSOLE_VGA) {
            term_clear();
        } else {
            console_print(ctx->output, "\n");
        }
    } else if (str_eq(trimmed, "heap")) {
        shell_print_heap_summary(ctx);
    } else if (str_startswith(trimmed, "alloc ")) {
        shell_handle_alloc(ctx, trimmed + 6);
    } else if (str_startswith(trimmed, "cd ")) {
        shell_handle_cd(ctx, trimmed + 3);
    } else if (str_eq(trimmed, "reboot")) {
        term_print("Rebooting...\n");
        try_reboot();
    } else if (str_eq(trimmed, "halt")) {
        term_print("CPU halted. Reset VM to continue.\n");
        halt_forever();
    } else {
        if (!shell_run_program(ctx, mbi, magic, trimmed)) {
            console_print(ctx->output, "Unknown command/program. Type \"help\".\n");
        }
    }
}

static void shell_process_char(struct shell_context *ctx, const struct multiboot_info *mbi, u32 magic, char c) {
    if (c == '\r') {
        c = '\n';
    }

    if (c == '\n') {
        ctx->input[ctx->len] = '\0';
        console_putchar(ctx->output, '\n');
        shell_execute_line(ctx, mbi, magic, ctx->input);
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

    if (c >= ' ' && c <= '~' && ctx->len < SHELL_INPUT_MAX) {
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
