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
    int pid;

    if (argc < 2) {
        write_str("svc_lookup: usage svc_lookup NAME\n");
        return 1;
    }

    pid = sys_service_lookup(argv[1]);
    if (pid < 0) {
        write_str("svc_lookup: not found\n");
        return 1;
    }

    write_str("svc_lookup: pid=");
    write_u32((unsigned int)pid);
    write_str("\n");
    return 0;
}
