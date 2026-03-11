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
    console_print(target, "PID  PPID TSK PRC TICKS NAME\n");
    for (i = 0; i < MAX_TASKS; ++i) {
        enum proc_state pstate;
        if (tasks[i].state == TASK_UNUSED) {
            continue;
        }

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
