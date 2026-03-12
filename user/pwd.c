#include "sys.h"

int main(void) {
    char buf[128];
    int n = sys_getcwd(buf, sizeof(buf));
    if (n < 0) {
        const char *msg = "pwd: failed\n";
        (void)sys_write(1, msg, cstr_len(msg));
        return 1;
    }
    (void)sys_write(1, buf, (unsigned int)n);
    (void)sys_write(1, "\n", 1u);
    return 0;
}
