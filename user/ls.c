#include "sys.h"

int main(int argc, char **argv) {
    const char *path = (const char *)0;
    if (argc >= 2) {
        path = argv[1];
    }
    return sys_list(path) < 0 ? 1 : 0;
}
