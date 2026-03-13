#include "kernel.h"

extern volatile u32 ring3_stop_reason;

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

static int exec_run_elf_path(const char *path, int argc, char **argv, struct exec_context *ctx) {
    const char *file_data = (const char *)0;
    u32 file_size = 0u;
    struct exec_load_plan plan;
    struct proc_image image;
    int rc = -1;
    int pid;
    int ppid;

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

    pid = proc_spawn_kernel("userprog", ppid);
    if (pid < 0) {
        console_print(ctx->output, "run: failed to allocate process\n");
        return 0;
    }

    if (!elf_load_from_plan(pid, &plan, file_data, file_size, &image, ctx->output)) {
        (void)proc_exit(pid, -1);
        (void)proc_reap_pid(pid, (int *)0);
        return 0;
    }
    (void)proc_bind_image(pid, &image);
    syscall_set_user_cwd(*ctx->cwd);
    syscall_set_bootinfo(ctx->mbi, ctx->magic);

    for (;;) {
        proc_set_state(pid, PROC_RUNNING);
        if (image.context_valid) {
            if (!elf_resume_image(&image, pid, &rc, ctx->output)) {
                (void)proc_exit(pid, -1);
                (void)proc_reap_pid(pid, (int *)0);
                return 0;
            }
        } else {
            if (!elf_execute_image(&image, pid, argc, argv, &rc, ctx->output)) {
                (void)proc_exit(pid, -1);
                (void)proc_reap_pid(pid, (int *)0);
                return 0;
            }
        }

        if (ring3_stop_reason == 1u) {
            if (!proc_get_image(pid, &image)) {
                (void)proc_exit(pid, -1);
                (void)proc_reap_pid(pid, (int *)0);
                return 0;
            }
            proc_set_state(pid, PROC_READY);
            continue;
        }
        break;
    }

    (void)proc_exit(pid, rc);
    (void)proc_reap_pid(pid, (int *)0);

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
