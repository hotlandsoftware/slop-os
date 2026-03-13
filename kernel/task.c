#include "kernel.h"

#define MAX_TASKS 32

static struct task_info tasks[MAX_TASKS];
static u32 tasks_used = 0;
static int current_index = -1;
static u32 switches = 0;
static u32 slice_ticks = 0;

#define SCHED_TIMESLICE_TICKS 4u

static int task_alloc_slot(void) {
    u32 i;
    for (i = 0; i < MAX_TASKS; ++i) {
        if (tasks[i].state == TASK_UNUSED || tasks[i].state == TASK_ZOMBIE) {
            return (int)i;
        }
    }
    return -1;
}

static int task_find_slot_by_pid(int pid) {
    u32 i;
    for (i = 0; i < MAX_TASKS; ++i) {
        if (tasks[i].state != TASK_UNUSED && tasks[i].pid == pid) {
            return (int)i;
        }
    }
    return -1;
}

static void task_return_to_kernel_main(void) {
    int kernel_slot = task_find_slot_by_pid(0);

    if (kernel_slot < 0) {
        current_index = -1;
        return;
    }

    if (current_index >= 0 && current_index != kernel_slot &&
        tasks[current_index].state == TASK_RUNNING) {
        proc_set_state(tasks[current_index].pid, PROC_READY);
        tasks[current_index].state = TASK_READY;
    }

    current_index = kernel_slot;
    tasks[kernel_slot].state = TASK_RUNNING;
    proc_set_state(tasks[kernel_slot].pid, PROC_RUNNING);
    slice_ticks = 0;
}

static const char *task_state_name(enum task_state state) {
    if (state == TASK_RUNNING) {
        return "RUN";
    }
    if (state == TASK_READY) {
        return "RDY";
    }
    if (state == TASK_BLOCKED) {
        return "BLK";
    }
    if (state == TASK_ZOMBIE) {
        return "ZMB";
    }
    return "UNK";
}

void tasking_init(void) {
    u32 i;
    for (i = 0; i < MAX_TASKS; ++i) {
        tasks[i].pid = 0;
        tasks[i].ppid = -1;
        tasks[i].state = TASK_UNUSED;
        tasks[i].runtime_ticks = 0;
        tasks[i].name[0] = '\0';
    }

    tasks_used = 0;
    switches = 0;
    slice_ticks = 0;

    current_index = task_alloc_slot();
    if (current_index >= 0) {
        tasks[current_index].pid = 0;
        tasks[current_index].ppid = -1;
        tasks[current_index].state = TASK_RUNNING;
        tasks[current_index].runtime_ticks = 0;
        str_copy(tasks[current_index].name, "kmain", sizeof(tasks[current_index].name));
        tasks_used = 1;
    }
}

int task_spawn_kernel(const char *name, int ppid) {
    int slot = task_alloc_slot();
    int pid;

    if (slot < 0) {
        return -1;
    }

    pid = proc_spawn_kernel(name, ppid);
    if (pid < 0) {
        return -1;
    }

    tasks[slot].pid = pid;
    tasks[slot].ppid = ppid;
    tasks[slot].state = TASK_READY;
    tasks[slot].runtime_ticks = 0;
    str_copy(tasks[slot].name, name ? name : "kproc", sizeof(tasks[slot].name));
    ++tasks_used;
    return pid;
}

int task_spawn_user(const char *name, int ppid) {
    int slot = task_alloc_slot();
    int pid;

    if (slot < 0) {
        return -1;
    }

    pid = proc_spawn_kernel(name, ppid);
    if (pid < 0) {
        return -1;
    }

    tasks[slot].pid = pid;
    tasks[slot].ppid = ppid;
    tasks[slot].state = TASK_READY;
    tasks[slot].runtime_ticks = 0;
    str_copy(tasks[slot].name, name ? name : "uproc", sizeof(tasks[slot].name));
    ++tasks_used;
    return pid;
}

void task_set_state(int pid, enum task_state state) {
    int slot = task_find_slot_by_pid(pid);
    if (slot < 0) {
        return;
    }
    if (state == TASK_RUNNING && current_index >= 0 && current_index != slot &&
        tasks[current_index].state == TASK_RUNNING) {
        proc_set_state(tasks[current_index].pid, PROC_READY);
        tasks[current_index].state = TASK_READY;
    }
    tasks[slot].state = state;
    if (state == TASK_RUNNING) {
        current_index = slot;
        slice_ticks = 0;
        proc_set_state(pid, PROC_RUNNING);
    }
}

int task_reap_pid(int pid) {
    int slot = task_find_slot_by_pid(pid);
    if (slot < 0) {
        return 0;
    }

    tasks[slot].pid = 0;
    tasks[slot].ppid = -1;
    tasks[slot].state = TASK_UNUSED;
    tasks[slot].runtime_ticks = 0;
    tasks[slot].name[0] = '\0';
    if (tasks_used > 0u) {
        --tasks_used;
    }
    if (current_index == slot) {
        task_return_to_kernel_main();
    }
    return 1;
}

int task_run_user_until_stop(int pid, int argc, char **argv, struct exec_context *ctx, int *out_exit_code, enum exec_stop_reason *out_stop_reason) {
    struct proc_image image;
    int rc = -1;
    extern volatile u32 ring3_stop_reason;

    if (pid <= 0 || !ctx || !proc_get_image(pid, &image) || !image.loaded) {
        return 0;
    }

    for (;;) {
        syscall_set_user_cwd(image.cwd ? image.cwd : (ctx->cwd ? *ctx->cwd : vfs_root()));
        syscall_set_bootinfo(ctx->mbi, ctx->magic);
        task_set_state(pid, TASK_RUNNING);
        proc_set_state(pid, PROC_RUNNING);

        if (image.context_valid) {
            if (!elf_resume_image(&image, pid, &rc, image.output)) {
                return 0;
            }
        } else {
            if (!elf_execute_image(&image, pid, argc, argv, &rc, image.output)) {
                return 0;
            }
        }

        if (ring3_stop_reason == 1u) {
            if (!proc_get_image(pid, &image)) {
                return 0;
            }
            if (!elf_snapshot_image(pid, &image)) {
                return 0;
            }
            task_set_state(pid, TASK_READY);
            proc_set_state(pid, PROC_READY);
            task_return_to_kernel_main();
            if (out_exit_code) {
                *out_exit_code = 0;
            }
            if (out_stop_reason) {
                *out_stop_reason = EXEC_STOP_YIELDED;
            }
            return 1;
        }
        if (ring3_stop_reason == 3u) {
            if (!proc_get_image(pid, &image)) {
                return 0;
            }
            if (!elf_snapshot_image(pid, &image)) {
                return 0;
            }
            task_set_state(pid, TASK_BLOCKED);
            proc_set_state(pid, PROC_BLOCKED);
            task_return_to_kernel_main();
            if (out_exit_code) {
                *out_exit_code = 0;
            }
            if (out_stop_reason) {
                *out_stop_reason = EXEC_STOP_BLOCKED;
            }
            return 1;
        }

        task_set_state(pid, TASK_ZOMBIE);
        proc_set_state(pid, PROC_ZOMBIE);
        task_return_to_kernel_main();
        if (out_exit_code) {
            *out_exit_code = rc;
        }
        if (out_stop_reason) {
            *out_stop_reason = EXEC_STOP_EXITED;
        }
        return 1;
    }
}

int task_resume_next_ready_user(const struct multiboot_info *mbi, u32 magic) {
    u32 i;
    for (i = 0; i < MAX_TASKS; ++i) {
        struct proc_image image;
        struct exec_context exec_ctx;
        enum exec_stop_reason stop_reason = EXEC_STOP_NONE;
        int exit_code = 0;

        if (tasks[i].state != TASK_READY || tasks[i].pid <= 0) {
            continue;
        }
        if (!proc_get_image(tasks[i].pid, &image) || !image.loaded || !image.context_valid) {
            continue;
        }

        exec_ctx.output = image.output;
        exec_ctx.cwd = &image.cwd;
        exec_ctx.mbi = mbi;
        exec_ctx.magic = magic;

        if (!task_run_user_until_stop(tasks[i].pid, 0, (char **)0, &exec_ctx, &exit_code, &stop_reason)) {
            return 0;
        }
        if (stop_reason == EXEC_STOP_EXITED) {
            (void)proc_exit(tasks[i].pid, exit_code);
            (void)proc_reap_pid(tasks[i].pid, (int *)0);
            (void)task_reap_pid(tasks[i].pid);
        }
        return 1;
    }
    return 0;
}

void scheduler_on_timer_tick(void) {
    u32 n;
    int next;

    if (current_index < 0) {
        return;
    }

    if (tasks[current_index].state == TASK_RUNNING) {
        ++tasks[current_index].runtime_ticks;
        ++slice_ticks;
    }

    if (tasks_used <= 1u) {
        return;
    }

    if (slice_ticks < SCHED_TIMESLICE_TICKS) {
        return;
    }

    for (n = 1; n < MAX_TASKS; ++n) {
        next = (current_index + (int)n) % MAX_TASKS;
        if (tasks[next].state == TASK_READY) {
            int old_pid = tasks[current_index].pid;
            int new_pid = tasks[next].pid;
            tasks[current_index].state = TASK_READY;
            tasks[next].state = TASK_RUNNING;
            current_index = next;
            proc_set_state(old_pid, PROC_READY);
            proc_set_state(new_pid, PROC_RUNNING);
            slice_ticks = 0;
            ++switches;
            break;
        }
    }
}

int task_current_pid(void) {
    if (current_index < 0) {
        return -1;
    }
    return tasks[current_index].pid;
}

u32 task_count(void) {
    return tasks_used;
}

u32 scheduler_switch_count(void) {
    return switches;
}

void task_list(enum console_target target) {
    u32 i;
    console_print(target, "CUR PID  PPID TSK PRC TICKS NAME\n");
    for (i = 0; i < MAX_TASKS; ++i) {
        enum proc_state pstate;
        if (tasks[i].state == TASK_UNUSED) {
            continue;
        }

        console_print(target, ((int)i == current_index) ? "*   " : "    ");
        console_print_u32_dec(target, (u32)tasks[i].pid);
        console_print(target, "    ");
        if (tasks[i].ppid < 0) {
            console_print(target, "-1");
        } else {
            console_print_u32_dec(target, (u32)tasks[i].ppid);
        }
        console_print(target, "   ");
        console_print(target, task_state_name(tasks[i].state));
        console_print(target, " ");
        pstate = proc_state_of(tasks[i].pid);
        console_print(target, proc_state_name(pstate));

        console_print(target, "    ");
        console_print_u32_dec(target, tasks[i].runtime_ticks);
        console_print(target, "    ");
        console_print(target, tasks[i].name);
        console_putchar(target, '\n');
    }
}
