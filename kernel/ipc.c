#include "kernel.h"

#define IPC_MAILBOX_MAX 32

struct ipc_mailbox {
    int used;
    int pid;
    int has_message;
    struct ipc_message message;
};

static struct ipc_mailbox mailboxes[IPC_MAILBOX_MAX];

struct ipc_waiter {
    int active;
    int pid;
    u32 user_msg_ptr;
};

static struct ipc_waiter waiters[IPC_MAILBOX_MAX];

static int ipc_find_slot(int pid) {
    u32 i;
    for (i = 0; i < IPC_MAILBOX_MAX; ++i) {
        if (mailboxes[i].used && mailboxes[i].pid == pid) {
            return (int)i;
        }
    }
    return -1;
}

static int ipc_alloc_slot(int pid) {
    u32 i;
    for (i = 0; i < IPC_MAILBOX_MAX; ++i) {
        if (!mailboxes[i].used) {
            mailboxes[i].used = 1;
            mailboxes[i].pid = pid;
            mailboxes[i].has_message = 0;
            return (int)i;
        }
    }
    return -1;
}

void ipc_init(void) {
    u32 i;
    for (i = 0; i < IPC_MAILBOX_MAX; ++i) {
        mailboxes[i].used = 0;
        mailboxes[i].pid = -1;
        mailboxes[i].has_message = 0;
        mem_zero(&mailboxes[i].message, sizeof(mailboxes[i].message));
        waiters[i].active = 0;
        waiters[i].pid = -1;
        waiters[i].user_msg_ptr = 0u;
    }
}

static int ipc_find_waiter(int pid) {
    u32 i;
    for (i = 0; i < IPC_MAILBOX_MAX; ++i) {
        if (waiters[i].active && waiters[i].pid == pid) {
            return (int)i;
        }
    }
    return -1;
}

static int ipc_alloc_waiter(int pid) {
    u32 i;
    for (i = 0; i < IPC_MAILBOX_MAX; ++i) {
        if (!waiters[i].active) {
            waiters[i].active = 1;
            waiters[i].pid = pid;
            waiters[i].user_msg_ptr = 0u;
            return (int)i;
        }
    }
    return -1;
}

int ipc_block_recv(int dst_pid, u32 user_msg_ptr) {
    int slot;

    if (dst_pid < 0 || proc_state_of(dst_pid) == PROC_UNUSED) {
        return 0;
    }

    slot = ipc_find_waiter(dst_pid);
    if (slot < 0) {
        slot = ipc_alloc_waiter(dst_pid);
        if (slot < 0) {
            return 0;
        }
    }

    waiters[slot].user_msg_ptr = user_msg_ptr;
    return 1;
}

int ipc_unblock_deliver(int dst_pid, const struct ipc_message *msg) {
    int slot;
    struct proc_image image;

    if (!msg || dst_pid < 0) {
        return 0;
    }

    slot = ipc_find_waiter(dst_pid);
    if (slot < 0) {
        return 0;
    }
    if (proc_state_of(dst_pid) == PROC_UNUSED) {
        waiters[slot].active = 0;
        waiters[slot].pid = -1;
        waiters[slot].user_msg_ptr = 0u;
        return 0;
    }
    if (!elf_write_to_task_memory(dst_pid, waiters[slot].user_msg_ptr, msg, sizeof(*msg))) {
        return 0;
    }
    if (!proc_get_image(dst_pid, &image) || !image.loaded || !image.context_valid) {
        return 0;
    }

    image.context.eax = 0u;
    (void)proc_bind_image(dst_pid, &image);
    task_set_state(dst_pid, TASK_READY);
    proc_set_state(dst_pid, PROC_READY);

    waiters[slot].active = 0;
    waiters[slot].pid = -1;
    waiters[slot].user_msg_ptr = 0u;
    return 1;
}

int ipc_send(int src_pid, int dst_pid, const struct ipc_message *msg) {
    int slot;
    struct ipc_message delivered;

    if (!msg || dst_pid < 0 || proc_state_of(dst_pid) == PROC_UNUSED) {
        return 0;
    }

    delivered = *msg;
    delivered.src_pid = src_pid;
    delivered.dst_pid = dst_pid;

    if (ipc_unblock_deliver(dst_pid, &delivered)) {
        return 1;
    }

    slot = ipc_find_slot(dst_pid);
    if (slot < 0) {
        slot = ipc_alloc_slot(dst_pid);
        if (slot < 0) {
            return 0;
        }
    }

    if (mailboxes[slot].has_message) {
        return 0;
    }

    mailboxes[slot].message = delivered;
    mailboxes[slot].has_message = 1;
    return 1;
}

int ipc_recv(int dst_pid, struct ipc_message *out_msg) {
    int slot;

    if (!out_msg || dst_pid < 0) {
        return 0;
    }

    slot = ipc_find_slot(dst_pid);
    if (slot < 0 || !mailboxes[slot].has_message) {
        return 0;
    }

    *out_msg = mailboxes[slot].message;
    mailboxes[slot].has_message = 0;
    mem_zero(&mailboxes[slot].message, sizeof(mailboxes[slot].message));
    return 1;
}

int ipc_reply(int src_pid, int dst_pid, const struct ipc_message *msg) {
    return ipc_send(src_pid, dst_pid, msg);
}
