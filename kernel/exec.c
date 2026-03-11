#include "kernel.h"

struct exec_program {
    const char *path;
    int (*entry)(int argc, char **argv, struct exec_context *ctx);
};

extern const u8 _binary_build_user_hello_elf_start[];
extern const u8 _binary_build_user_hello_elf_end[];
extern const u8 _binary_build_user_cat_elf_start[];
extern const u8 _binary_build_user_cat_elf_end[];
extern const u8 _binary_build_user_touch_elf_start[];
extern const u8 _binary_build_user_touch_elf_end[];

static int exec_is_elf_path(struct vfs_node *cwd, const char *path) {
    const char *data = (const char *)0;
    u32 size = 0u;
    if (!vfs_read_file(cwd, path, &data, &size) || !data || size < 4u) {
        return 0;
    }
    return (u8)data[0] == 0x7Fu &&
           (u8)data[1] == (u8)'E' &&
           (u8)data[2] == (u8)'L' &&
           (u8)data[3] == (u8)'F';
}

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

static int exec_run_elf_path(const char *path, int argc, char **argv, struct exec_context *ctx, int print_status) {
    struct proc_image image;
    int rc = -1;
    int exit_code = -1;
    int pid;
    int ppid;

    ppid = task_current_pid();
    if (ppid < 0) {
        ppid = 0;
    }

    pid = proc_spawn_kernel("userprog", ppid);
    if (pid < 0) {
        console_print(ctx->output, "run: failed to allocate process\n");
        return 0;
    }

    if (!elf_load_from_vfs(*ctx->cwd, path, pid, &image, ctx->output)) {
        (void)proc_exit(pid, -1);
        (void)proc_reap_pid(pid, (int *)0);
        return 0;
    }
    (void)proc_bind_image(pid, &image);
    proc_set_state(pid, PROC_RUNNING);
    syscall_set_user_cwd(*ctx->cwd);

    if (!elf_execute_image(&image, pid, argc, argv, &rc, ctx->output)) {
        (void)proc_exit(pid, -1);
        (void)proc_reap_pid(pid, (int *)0);
        return 0;
    }
    (void)proc_exit(pid, rc);
    (void)proc_reap_pid(pid, &exit_code);

    if (print_status) {
        console_print(ctx->output, "run: ");
        console_print(ctx->output, path);
        console_print(ctx->output, " returned ");
        if (exit_code < 0) {
            console_print(ctx->output, "-");
            console_print_u32_dec(ctx->output, (u32)(-exit_code));
        } else {
            console_print_u32_dec(ctx->output, (u32)exit_code);
        }
        console_putchar(ctx->output, '\n');
    }

    return 1;
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

static int prog_ps(int argc, char **argv, struct exec_context *ctx) {
    (void)argc;
    (void)argv;
    task_list(ctx->output);
    return 0;
}

static int prog_run(int argc, char **argv, struct exec_context *ctx) {
    if (argc < 2) {
        console_print(ctx->output, "run: usage run PATH\n");
        return -1;
    }
    return exec_run_elf_path(argv[1], argc - 1, &argv[1], ctx, 1) ? 0 : -1;
}

static int prog_ring3test(int argc, char **argv, struct exec_context *ctx) {
    (void)argc;
    (void)argv;
    if (!ring3_test(ctx->output)) {
        console_print(ctx->output, "ring3test: failed\n");
        return -1;
    }
    return 0;
}

static const struct exec_program programs[] = {
    {"/bin/pwd", prog_pwd},
    {"/bin/ls", prog_ls},
    {"/bin/mkdir", prog_mkdir},
    {"/bin/systeminfo", prog_systeminfo},
    {"/bin/mounts", prog_mounts},
    {"/bin/ps", prog_ps},
    {"/bin/run", prog_run},
    {"/bin/ring3test", prog_ring3test}
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
    {
        u32 size = (u32)(_binary_build_user_hello_elf_end - _binary_build_user_hello_elf_start);
        (void)vfs_write_file(vfs_root(), "/bin/hello.elf", (const char *)_binary_build_user_hello_elf_start, size);
    }
    {
        u32 size = (u32)(_binary_build_user_cat_elf_end - _binary_build_user_cat_elf_start);
        (void)vfs_write_file(vfs_root(), "/bin/cat", (const char *)_binary_build_user_cat_elf_start, size);
    }
    {
        u32 size = (u32)(_binary_build_user_touch_elf_end - _binary_build_user_touch_elf_start);
        (void)vfs_write_file(vfs_root(), "/bin/touch", (const char *)_binary_build_user_touch_elf_start, size);
    }
}

int exec_run_path(const char *path, int argc, char **argv, struct exec_context *ctx) {
    u32 i;
    for (i = 0; i < (sizeof(programs) / sizeof(programs[0])); ++i) {
        const struct exec_program *p = &programs[i];
        if (str_eq(path, p->path)) {
            (void)p->entry(argc, argv, ctx);
            return 1;
        }
    }
    if (exec_is_elf_path(*ctx->cwd, path) && exec_run_elf_path(path, argc, argv, ctx, 0)) {
        return 1;
    }
    return 0;
}
