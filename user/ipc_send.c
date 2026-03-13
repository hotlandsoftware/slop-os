#include "sys.h"

static void write_str(const char *s) {
    (void)sys_write(1, s, cstr_len(s));
}

static int parse_u32(const char *s, unsigned int *out) {
    unsigned int value = 0u;
    unsigned int i = 0u;

    if (!s || !out || s[0] == '\0') {
        return 0;
    }

    while (s[i] != '\0') {
        char c = s[i++];
        if (c < '0' || c > '9') {
            return 0;
        }
        value = (value * 10u) + (unsigned int)(c - '0');
    }

    *out = value;
    return 1;
}

int main(int argc, char **argv) {
    struct ipc_message msg;
    unsigned int dst_pid = 0u;
    unsigned int type = 1u;
    unsigned int arg1 = 111u;
    unsigned int arg2 = 222u;

    if (argc < 2 || !parse_u32(argv[1], &dst_pid)) {
        write_str("ipc_send: usage ipc_send PID [TYPE] [ARG1] [ARG2]\n");
        return 1;
    }
    if (argc >= 3 && !parse_u32(argv[2], &type)) {
        write_str("ipc_send: bad TYPE\n");
        return 1;
    }
    if (argc >= 4 && !parse_u32(argv[3], &arg1)) {
        write_str("ipc_send: bad ARG1\n");
        return 1;
    }
    if (argc >= 5 && !parse_u32(argv[4], &arg2)) {
        write_str("ipc_send: bad ARG2\n");
        return 1;
    }

    msg.src_pid = 0;
    msg.dst_pid = (int)dst_pid;
    msg.type = type;
    msg.arg1 = arg1;
    msg.arg2 = arg2;
    msg.arg3 = 0u;
    msg.arg4 = 0u;

    if (sys_ipc_send(&msg) < 0) {
        write_str("ipc_send: send failed\n");
        return 1;
    }

    write_str("ipc_send: sent\n");
    return 0;
}
