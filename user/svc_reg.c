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

int main(int argc, char **argv) {
    struct ipc_message msg;
    int pid;

    if (argc < 2) {
        write_str("svc_reg: usage svc_reg NAME\n");
        return 1;
    }
    if (sys_service_register(argv[1]) < 0) {
        write_str("svc_reg: register failed\n");
        return 1;
    }

    pid = sys_getpid();
    write_str("svc_reg: registered pid=");
    write_u32((unsigned int)pid);
    write_str(" name=");
    write_str(argv[1]);
    write_str("\n");
    write_str("svc_reg: waiting for ipc\n");

    if (sys_ipc_recv(&msg) < 0) {
        write_str("svc_reg: receive failed\n");
        return 1;
    }

    write_str("svc_reg: got ipc, exiting\n");
    return 0;
}
