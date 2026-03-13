#include "sys.h"

int main(void) {
    const char *before = "yieldtest: before yield\n";
    const char *after = "yieldtest: after yield\n";

    (void)sys_write(1, before, cstr_len(before));
    (void)sys_yield();
    (void)sys_write(1, after, cstr_len(after));
    return 0;
}
