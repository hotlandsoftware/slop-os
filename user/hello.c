static int sys_write(int fd, const char *buf, unsigned int len) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(1), "b"(fd), "c"(buf), "d"(len)
        : "memory");
    return ret;
}

int main(void) {
    const char *msg = "hello from /bin/hello.elf (user mode)\n";
    (void)sys_write(1, msg, 38u);
    return 42;
}
