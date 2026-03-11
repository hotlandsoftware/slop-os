static int sys_open(const char *path, unsigned int flags) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(2), "b"(path), "c"(flags)
        : "memory");
    return ret;
}

static int sys_read(int fd, char *buf, unsigned int len) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(0), "b"(fd), "c"(buf), "d"(len)
        : "memory");
    return ret;
}

static int sys_write(int fd, const char *buf, unsigned int len) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(1), "b"(fd), "c"(buf), "d"(len)
        : "memory");
    return ret;
}

static int sys_close(int fd) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(3), "b"(fd)
        : "memory");
    return ret;
}

static unsigned int cstr_len(const char *s) {
    unsigned int n = 0u;
    while (s && s[n] != '\0') {
        ++n;
    }
    return n;
}

int main(int argc, char **argv) {
    int fd;
    char buf[128];
    int n;
    const char *usage = "cat: usage cat PATH\n";

    if (argc < 2) {
        (void)sys_write(1, usage, cstr_len(usage));
        return 1;
    }

    fd = sys_open(argv[1], 0u);
    if (fd < 0) {
        const char *msg = "cat: no such file\n";
        (void)sys_write(1, msg, cstr_len(msg));
        return 1;
    }

    for (;;) {
        n = sys_read(fd, buf, sizeof(buf));
        if (n <= 0) {
            break;
        }
        (void)sys_write(1, buf, (unsigned int)n);
    }
    (void)sys_close(fd);
    (void)sys_write(1, "\n", 1u);
    return 0;
}
