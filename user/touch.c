static int sys_open(const char *path, unsigned int flags) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(2), "b"(path), "c"(flags)
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

static int sys_write(int fd, const char *buf, unsigned int len) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(1), "b"(fd), "c"(buf), "d"(len)
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
    const char *usage = "touch: usage touch PATH\n";

    if (argc < 2) {
        (void)sys_write(1, usage, cstr_len(usage));
        return 1;
    }

    fd = sys_open(argv[1], 0x40u);
    if (fd < 0) {
        const char *msg = "touch: failed\n";
        (void)sys_write(1, msg, cstr_len(msg));
        return 1;
    }

    (void)sys_close(fd);
    return 0;
}
