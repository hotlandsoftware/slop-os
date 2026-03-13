#include "sys.h"

#define SYSINFO_SERVICE "sysinfo"
#define SYSINFO_MSG_REQUEST 1u

static void write_str(const char *s) {
    (void)sys_write(1, s, cstr_len(s));
}

int main(void) {
    struct ipc_message msg;
    int service_pid = sys_service_lookup(SYSINFO_SERVICE);

    if (service_pid < 0) {
        write_str("systeminfo: sysinfo service not found\n");
        write_str("systeminfo: start sysinfod first\n");
        return 1;
    }

    msg.src_pid = 0;
    msg.dst_pid = service_pid;
    msg.type = SYSINFO_MSG_REQUEST;
    msg.arg1 = 0u;
    msg.arg2 = 0u;
    msg.arg3 = 0u;
    msg.arg4 = 0u;

    if (sys_ipc_send(&msg) < 0) {
        write_str("systeminfo: request failed\n");
        return 1;
    }

    return 0;
}
