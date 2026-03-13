#include "sys.h"

static void write_str(const char *s) {
    (void)sys_write(1, s, cstr_len(s));
}

static void write_u32(unsigned int value) {
    char buf[10];
    unsigned int i = 0u;

    if (value == 0u) {
        (void)sys_write(1, "0", 1u);
        return;
    }

    while (value > 0u && i < sizeof(buf)) {
        buf[i++] = (char)('0' + (value % 10u));
        value /= 10u;
    }

    while (i > 0u) {
        char c = buf[--i];
        (void)sys_write(1, &c, 1u);
    }
}

int main(void) {
    struct ipc_message msg;
    int pid = sys_getpid();

    write_str("ipc_recv pid=");
    write_u32((unsigned int)pid);
    write_str("\n");
    if (sys_ipc_recv(&msg) < 0) {
        write_str("ipc_recv: no message\n");
        return 1;
    }

    write_str("ipc_recv got message: src=");
    write_u32((unsigned int)msg.src_pid);
    write_str(" type=");
    write_u32(msg.type);
    write_str(" arg1=");
    write_u32(msg.arg1);
    write_str(" arg2=");
    write_u32(msg.arg2);
    write_str("\n");
    return 0;
}
