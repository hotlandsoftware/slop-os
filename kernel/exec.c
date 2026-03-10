#include "kernel.h"

#define EXEC_MAX_LINE 128

struct exec_program {
    const char *path;
    int (*entry)(int argc, char **argv, struct exec_context *ctx);
};

static int prog_pwd(int argc, char **argv, struct exec_context *ctx) {
    char path[128];
    (void)argc;
    (void)argv;
    vfs_get_cwd_path(*ctx->cwd, path, sizeof(path));
    console_printf(ctx->output, "%s\n", path);
    return 0;
}

static int prog_ls(int argc, char **argv, struct exec_context *ctx) {
    if (argc >= 2) {
        vfs_list(*ctx->cwd, argv[1], ctx->output);
    } else {
        vfs_list(*ctx->cwd, (const char *)0, ctx->output);
    }
    return 0;
}

static int prog_mkdir(int argc, char **argv, struct exec_context *ctx) {
    if (argc < 2) {
        console_print(ctx->output, "mkdir: usage mkdir PATH\n");
        return -1;
    }
    if (!vfs_make_dir(*ctx->cwd, argv[1])) {
        console_print(ctx->output, "mkdir: failed\n");
        return -1;
    }
    return 0;
}

static int prog_touch(int argc, char **argv, struct exec_context *ctx) {
    if (argc < 2) {
        console_print(ctx->output, "touch: usage touch PATH\n");
        return -1;
    }
    if (!vfs_touch(*ctx->cwd, argv[1])) {
        console_print(ctx->output, "touch: failed\n");
        return -1;
    }
    return 0;
}

static int prog_cat(int argc, char **argv, struct exec_context *ctx) {
    const char *data;
    u32 size;
    if (argc < 2) {
        console_print(ctx->output, "cat: usage cat PATH\n");
        return -1;
    }
    if (!vfs_read_file(*ctx->cwd, argv[1], &data, &size)) {
        console_print(ctx->output, "cat: no such file\n");
        return -1;
    }
    if (size > 0u && data) {
        console_print(ctx->output, data);
    }
    console_putchar(ctx->output, '\n');
    return 0;
}

static int prog_echo(int argc, char **argv, struct exec_context *ctx) {
    char out[EXEC_MAX_LINE];
    u32 used = 0;
    int i;
    int redir = -1;

    for (i = 1; i < argc; ++i) {
        if (str_eq(argv[i], ">")) {
            redir = i;
            break;
        }
    }

    if (redir == 1) {
        console_print(ctx->output, "echo: missing text\n");
        return -1;
    }

    if (redir >= 0 && redir + 1 >= argc) {
        console_print(ctx->output, "echo: missing redirect path\n");
        return -1;
    }

    out[0] = '\0';
    for (i = 1; i < argc; ++i) {
        u32 j;
        const char *part = argv[i];
        if (i == redir) {
            break;
        }
        if (used != 0u) {
            if (used + 1u >= sizeof(out)) {
                break;
            }
            out[used++] = ' ';
            out[used] = '\0';
        }
        for (j = 0; part[j] != '\0'; ++j) {
            if (used + 1u >= sizeof(out)) {
                break;
            }
            out[used++] = part[j];
            out[used] = '\0';
        }
    }

    if (redir >= 0) {
        if (!vfs_write_file(*ctx->cwd, argv[redir + 1], out, used)) {
            console_print(ctx->output, "echo: write failed\n");
            return -1;
        }
        return 0;
    }

    console_print(ctx->output, out);
    console_putchar(ctx->output, '\n');
    return 0;
}

static int prog_systeminfo(int argc, char **argv, struct exec_context *ctx) {
    (void)argc;
    (void)argv;
    print_systeminfo(ctx->mbi, ctx->magic);
    return 0;
}

static int prog_mounts(int argc, char **argv, struct exec_context *ctx) {
    (void)argc;
    (void)argv;
    fs_print_mounts(ctx->output);
    return 0;
}

static const struct exec_program programs[] = {
    {"/bin/pwd", prog_pwd},
    {"/bin/ls", prog_ls},
    {"/bin/mkdir", prog_mkdir},
    {"/bin/touch", prog_touch},
    {"/bin/cat", prog_cat},
    {"/bin/echo", prog_echo},
    {"/bin/systeminfo", prog_systeminfo},
    {"/bin/mounts", prog_mounts}
};

void exec_seed_programs(void) {
    u32 i;
    const char *seed =
        "#!slop-exec\n"
        "stub user-program placeholder\n";
    for (i = 0; i < (sizeof(programs) / sizeof(programs[0])); ++i) {
        const struct exec_program *p = &programs[i];
        (void)vfs_write_file(vfs_root(), p->path, seed, str_len(seed));
    }
}

int exec_run_path(const char *path, int argc, char **argv, struct exec_context *ctx) {
    u32 i;
    for (i = 0; i < (sizeof(programs) / sizeof(programs[0])); ++i) {
        const struct exec_program *p = &programs[i];
        if (str_eq(path, p->path)) {
            return p->entry(argc, argv, ctx) == 0;
        }
    }
    return 0;
}
