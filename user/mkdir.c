#include "sys.h"

int main(int argc, char **argv) {
    const char *usage = "mkdir: usage mkdir PATH\n";
    if (argc < 2) {
        (void)sys_write(1, usage, cstr_len(usage));
        return 1;
    }
    if (sys_mkdir(argv[1]) < 0) {
        const char *msg = "mkdir: failed\n";
        (void)sys_write(1, msg, cstr_len(msg));
        return 1;
    }
    return 0;
}
