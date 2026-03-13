#include "kernel.h"

struct exec_program {
    const char *path;
    int (*entry)(int argc, char **argv, struct exec_context *ctx);
};
static int exec_has_suffix(const char *s, const char *suffix) {
    u32 s_len = str_len(s);
    u32 suf_len = str_len(suffix);
    u32 i;
    if (s_len < suf_len) {
        return 0;
    }
    for (i = 0u; i < suf_len; ++i) {
        if (s[s_len - suf_len + i] != suffix[i]) {
            return 0;
        }
    }
    return 1;
}

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

static void exec_program_name_from_path(const char *path, char *out, u32 out_size) {
    const char *name = path;
    u32 len = 0u;

    if (!out || out_size == 0u) {
        return;
    }

    if (!path || path[0] == '\0') {
        str_copy(out, "userprog", out_size);
        return;
    }

    while (*path != '\0') {
        if (*path == '/' && path[1] != '\0') {
            name = path + 1;
        }
        ++path;
    }

    while (name[len] != '\0') {
        ++len;
    }
    if (len > 4u &&
        name[len - 4u] == '.' &&
        name[len - 3u] == 'e' &&
        name[len - 2u] == 'l' &&
        name[len - 1u] == 'f') {
        len -= 4u;
    }

    if (len == 0u) {
        str_copy(out, "userprog", out_size);
        return;
    }

    if (len + 1u > out_size) {
        len = out_size - 1u;
    }

    {
        u32 i;
        for (i = 0u; i < len; ++i) {
            out[i] = name[i];
        }
        out[len] = '\0';
    }
}

static int exec_run_elf_path(const char *path, int argc, char **argv, struct exec_context *ctx) {
    const char *file_data = (const char *)0;
    u32 file_size = 0u;
    struct exec_load_plan plan;
    struct proc_image image;
    char prog_name[16];
    int rc = -1;
    int pid;
    int ppid;
    enum exec_stop_reason stop_reason = EXEC_STOP_NONE;

    ppid = task_current_pid();
    if (ppid < 0) {
        ppid = 0;
    }

    if (!vfs_read_file(*ctx->cwd, path, &file_data, &file_size) || !file_data) {
        console_print(ctx->output, "run: file not found\n");
        return 0;
    }

    if (!elf_prepare_load_plan_from_vfs(*ctx->cwd, path, &plan, ctx->output)) {
        return 0;
    }

    exec_program_name_from_path(path, prog_name, sizeof(prog_name));
    pid = task_spawn_user(prog_name, ppid);
    if (pid < 0) {
        console_print(ctx->output, "run: failed to allocate process\n");
        return 0;
    }

    if (!elf_load_from_plan(pid, &plan, file_data, file_size, &image, ctx->output)) {
        (void)proc_exit(pid, -1);
        (void)proc_reap_pid(pid, (int *)0);
        (void)task_reap_pid(pid);
        return 0;
    }
    image.cwd = *ctx->cwd;
    image.output = ctx->output;
    (void)proc_bind_image(pid, &image);

    if (!task_run_user_until_stop(pid, argc, argv, ctx, &rc, &stop_reason)) {
        (void)proc_exit(pid, -1);
        (void)proc_reap_pid(pid, (int *)0);
        (void)task_reap_pid(pid);
        return 0;
    }

    if (stop_reason == EXEC_STOP_YIELDED) {
        console_print(ctx->output, "run: yielded pid ");
        console_print_u32_dec(ctx->output, (u32)pid);
        console_print(ctx->output, "\n");
        return 1;
    }
    if (stop_reason == EXEC_STOP_BLOCKED) {
        console_print(ctx->output, "run: blocked pid ");
        console_print_u32_dec(ctx->output, (u32)pid);
        console_print(ctx->output, "\n");
        return 1;
    }

    (void)proc_exit(pid, rc);
    (void)proc_reap_pid(pid, (int *)0);
    (void)task_reap_pid(pid);

    return 1;
}

static int prog_mounts(int argc, char **argv, struct exec_context *ctx) {
    (void)argc;
    (void)argv;
    fs_print_mounts(ctx->output);
    return 0;
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
    {"/bin/mounts", prog_mounts},
    {"/bin/ring3test", prog_ring3test}
};

int exec_seed_programs(void) {
    return 0;
}

int exec_run_path(const char *path, int argc, char **argv, struct exec_context *ctx) {
    u32 i;
    char alt_path[128];
    u32 j;
    for (i = 0; i < (sizeof(programs) / sizeof(programs[0])); ++i) {
        const struct exec_program *p = &programs[i];
        if (str_eq(path, p->path)) {
            (void)p->entry(argc, argv, ctx);
            return 1;
        }
    }

    if (exec_is_elf_path(*ctx->cwd, path)) {
        return exec_run_elf_path(path, argc, argv, ctx) ? 1 : -1;
    }

    if (!exec_has_suffix(path, ".elf")) {
        str_copy(alt_path, path, sizeof(alt_path));
        j = str_len(alt_path);
        if (j + 4u < sizeof(alt_path)) {
            alt_path[j++] = '.';
            alt_path[j++] = 'e';
            alt_path[j++] = 'l';
            alt_path[j++] = 'f';
            alt_path[j] = '\0';
            if (exec_is_elf_path(*ctx->cwd, alt_path)) {
                return exec_run_elf_path(alt_path, argc, argv, ctx) ? 1 : -1;
            }
        }
    }

    if (str_startswith(path, "/bin/")) {
        str_copy(alt_path, "/mount/cdrom", sizeof(alt_path));
        j = str_len(alt_path);
        {
            u32 k = 0u;
            while (path[k] != '\0' && (j + 1u) < sizeof(alt_path)) {
                alt_path[j++] = path[k++];
            }
            alt_path[j] = '\0';
        }
        if (exec_is_elf_path(*ctx->cwd, alt_path)) {
            return exec_run_elf_path(alt_path, argc, argv, ctx) ? 1 : -1;
        }

        if (!exec_has_suffix(alt_path, ".elf")) {
            j = str_len(alt_path);
            if (j + 4u < sizeof(alt_path)) {
                alt_path[j++] = '.';
                alt_path[j++] = 'e';
                alt_path[j++] = 'l';
                alt_path[j++] = 'f';
                alt_path[j] = '\0';
                if (exec_is_elf_path(*ctx->cwd, alt_path)) {
                    return exec_run_elf_path(alt_path, argc, argv, ctx) ? 1 : -1;
                }
            }
        }
    }

    return 0;
}

int exec_resume_pid(int pid, struct exec_context *ctx) {
    int rc = -1;
    enum exec_stop_reason stop_reason = EXEC_STOP_NONE;
    struct proc_image image;

    if (pid <= 0 || !ctx) {
        return 0;
    }
    if (!proc_get_image(pid, &image) || !image.loaded || !image.context_valid) {
        console_print(ctx->output, "resume: pid not resumable\n");
        return 0;
    }

    if (!task_run_user_until_stop(pid, 0, (char **)0, ctx, &rc, &stop_reason)) {
        console_print(ctx->output, "resume: failed\n");
        return 0;
    }

    if (stop_reason == EXEC_STOP_YIELDED) {
        console_print(ctx->output, "resume: yielded pid ");
        console_print_u32_dec(ctx->output, (u32)pid);
        console_print(ctx->output, "\n");
        return 1;
    }
    if (stop_reason == EXEC_STOP_BLOCKED) {
        console_print(ctx->output, "resume: blocked pid ");
        console_print_u32_dec(ctx->output, (u32)pid);
        console_print(ctx->output, "\n");
        return 1;
    }

    (void)proc_exit(pid, rc);
    (void)proc_reap_pid(pid, (int *)0);
    (void)task_reap_pid(pid);
    return 1;
}
