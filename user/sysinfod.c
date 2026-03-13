#include "sys.h"

#define SYSINFO_SERVICE "sysinfo"
#define SYSINFO_MSG_REQUEST 1u

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

    if (sys_service_register(SYSINFO_SERVICE) < 0) {
        write_str("sysinfod: register failed\n");
        return 1;
    }

    write_str("sysinfod: registered pid=");
    write_u32((unsigned int)pid);
    write_str(" name=");
    write_str(SYSINFO_SERVICE);
    write_str("\n");

    for (;;) {
        if (sys_ipc_recv(&msg) < 0) {
            write_str("sysinfod: receive failed\n");
            return 1;
        }

        if (msg.type == SYSINFO_MSG_REQUEST) {
            (void)sys_systeminfo();
        }
    }
}
