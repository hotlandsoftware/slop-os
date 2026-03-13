#include "kernel.h"

#define IPC_MAILBOX_MAX 32

struct ipc_mailbox {
    int used;
    int pid;
    int has_message;
    struct ipc_message message;
};

static struct ipc_mailbox mailboxes[IPC_MAILBOX_MAX];

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
    }
}

int ipc_send(int src_pid, int dst_pid, const struct ipc_message *msg) {
    int slot;

    if (!msg || dst_pid < 0 || proc_state_of(dst_pid) == PROC_UNUSED) {
        return 0;
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

    mailboxes[slot].message = *msg;
    mailboxes[slot].message.src_pid = src_pid;
    mailboxes[slot].message.dst_pid = dst_pid;
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
