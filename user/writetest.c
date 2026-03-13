#include "sys.h"

#define O_WRONLY 0x1u
#define O_CREAT 0x40u
#define O_TRUNC 0x200u

static void write_str(const char *s) {
    (void)sys_write(1, s, cstr_len(s));
}

int main(int argc, char **argv) {
    int fd;
    const char *text;

    if (argc < 3) {
        write_str("writetest: usage writetest PATH TEXT\n");
        return 1;
    }

    fd = sys_open(argv[1], O_WRONLY | O_CREAT | O_TRUNC);
    if (fd < 0) {
        write_str("writetest: open failed\n");
        return 1;
    }

    text = argv[2];
    if (sys_write(fd, text, cstr_len(text)) < 0) {
        write_str("writetest: write failed\n");
        (void)sys_close(fd);
        return 1;
    }

    if (sys_close(fd) < 0) {
        write_str("writetest: close failed\n");
        return 1;
    }

    write_str("writetest: ok\n");
    return 0;
}
