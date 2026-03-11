#include "kernel.h"

#define MAX_PROCS 32

static struct process_info procs[MAX_PROCS];
static struct proc_image images[MAX_PROCS];
static u32 used = 0;
static int next_pid = 1;

static int find_slot_by_pid(int pid) {
    u32 i;
    for (i = 0; i < MAX_PROCS; ++i) {
        if (procs[i].state != PROC_UNUSED && procs[i].pid == pid) {
            return (int)i;
        }
    }
    return -1;
}

static int alloc_slot(void) {
    u32 i;
    for (i = 0; i < MAX_PROCS; ++i) {
        if (procs[i].state == PROC_UNUSED) {
            return (int)i;
        }
    }
    return -1;
}

void proc_init(void) {
    u32 i;
    int slot;

    for (i = 0; i < MAX_PROCS; ++i) {
        procs[i].pid = -1;
        procs[i].ppid = -1;
        procs[i].state = PROC_UNUSED;
        procs[i].exit_code = 0;
        procs[i].name[0] = '\0';
        images[i].loaded = 0;
        images[i].entry = 0;
        images[i].image_base = 0;
        images[i].image_size = 0;
        images[i].user_stack_base = 0;
        images[i].user_stack_size = 0;
    }

    used = 0;
    next_pid = 1;

    slot = alloc_slot();
    if (slot >= 0) {
        procs[slot].pid = 0;
        procs[slot].ppid = -1;
        procs[slot].state = PROC_RUNNING;
        procs[slot].exit_code = 0;
        str_copy(procs[slot].name, "kmain", sizeof(procs[slot].name));
        used = 1;
    }
}

int proc_spawn_kernel(const char *name, int ppid) {
    int slot = alloc_slot();
    if (slot < 0) {
        return -1;
    }

    procs[slot].pid = next_pid++;
    procs[slot].ppid = ppid;
    procs[slot].state = PROC_READY;
    procs[slot].exit_code = 0;
    str_copy(procs[slot].name, name ? name : "kproc", sizeof(procs[slot].name));
    images[slot].loaded = 0;
    images[slot].entry = 0;
    images[slot].image_base = 0;
    images[slot].image_size = 0;
    images[slot].user_stack_base = 0;
    images[slot].user_stack_size = 0;
    ++used;
    return procs[slot].pid;
}

int proc_exit(int pid, int code) {
    int slot = find_slot_by_pid(pid);
    if (slot < 0) {
        return 0;
    }
    procs[slot].state = PROC_ZOMBIE;
    procs[slot].exit_code = code;
    return 1;
}

int proc_wait(int ppid, int *child_pid, int *exit_code) {
    u32 i;
    for (i = 0; i < MAX_PROCS; ++i) {
        if (procs[i].state == PROC_ZOMBIE && procs[i].ppid == ppid) {
            if (child_pid) {
                *child_pid = procs[i].pid;
            }
            if (exit_code) {
                *exit_code = procs[i].exit_code;
            }
            procs[i].pid = -1;
            procs[i].ppid = -1;
            procs[i].state = PROC_UNUSED;
            procs[i].exit_code = 0;
            procs[i].name[0] = '\0';
            images[i].loaded = 0;
            images[i].entry = 0;
            images[i].image_base = 0;
            images[i].image_size = 0;
            images[i].user_stack_base = 0;
            images[i].user_stack_size = 0;
            if (used > 0u) {
                --used;
            }
            return 1;
        }
    }
    return 0;
}

void proc_set_state(int pid, enum proc_state state) {
    int slot = find_slot_by_pid(pid);
    if (slot < 0) {
        return;
    }
    procs[slot].state = state;
}

enum proc_state proc_state_of(int pid) {
    int slot = find_slot_by_pid(pid);
    if (slot < 0) {
        return PROC_UNUSED;
    }
    return procs[slot].state;
}

const char *proc_state_name(enum proc_state state) {
    if (state == PROC_RUNNING) {
        return "RUN";
    }
    if (state == PROC_READY) {
        return "RDY";
    }
    if (state == PROC_BLOCKED) {
        return "BLK";
    }
    if (state == PROC_ZOMBIE) {
        return "ZMB";
    }
    return "UNK";
}

u32 proc_count(void) {
    return used;
}

int proc_bind_image(int pid, const struct proc_image *image) {
    int slot = find_slot_by_pid(pid);
    if (slot < 0 || !image) {
        return 0;
    }
    images[slot] = *image;
    return 1;
}

int proc_get_image(int pid, struct proc_image *image) {
    int slot = find_slot_by_pid(pid);
    if (slot < 0 || !image) {
        return 0;
    }
    *image = images[slot];
    return 1;
}

int proc_reap_pid(int pid, int *exit_code) {
    int slot = find_slot_by_pid(pid);
    if (slot < 0 || procs[slot].state != PROC_ZOMBIE) {
        return 0;
    }

    if (exit_code) {
        *exit_code = procs[slot].exit_code;
    }

    procs[slot].pid = -1;
    procs[slot].ppid = -1;
    procs[slot].state = PROC_UNUSED;
    procs[slot].exit_code = 0;
    procs[slot].name[0] = '\0';
    images[slot].loaded = 0;
    images[slot].entry = 0;
    images[slot].image_base = 0;
    images[slot].image_size = 0;
    images[slot].user_stack_base = 0;
    images[slot].user_stack_size = 0;
    if (used > 0u) {
        --used;
    }
    return 1;
}
