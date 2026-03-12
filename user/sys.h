#ifndef SLOP_USER_SYS_H
#define SLOP_USER_SYS_H

struct mem_info {
    unsigned int mem_total_kib;
    unsigned int mem_used_kib;
    unsigned int mem_free_kib;
    unsigned int mem_shared_kib;
    unsigned int mem_buff_cache_kib;
    unsigned int mem_available_kib;
    unsigned int swap_total_kib;
    unsigned int swap_used_kib;
    unsigned int swap_free_kib;
};

static inline int sys_read(int fd, char *buf, unsigned int len) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(0), "b"(fd), "c"(buf), "d"(len)
        : "memory");
    return ret;
}

static inline int sys_write(int fd, const char *buf, unsigned int len) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(1), "b"(fd), "c"(buf), "d"(len)
        : "memory");
    return ret;
}

static inline int sys_open(const char *path, unsigned int flags) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(2), "b"(path), "c"(flags)
        : "memory");
    return ret;
}

static inline int sys_close(int fd) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(3), "b"(fd)
        : "memory");
    return ret;
}

static inline int sys_mkdir(const char *path) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(83), "b"(path)
        : "memory");
    return ret;
}

static inline int sys_getcwd(char *buf, unsigned int len) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(183), "b"(buf), "c"(len)
        : "memory");
    return ret;
}

static inline int sys_list(const char *path) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(241), "b"(path)
        : "memory");
    return ret;
}

static inline int sys_systeminfo(void) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(242)
        : "memory");
    return ret;
}

static inline int sys_ps(void) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(243)
        : "memory");
    return ret;
}

static inline int sys_meminfo(struct mem_info *info) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(244), "b"(info)
        : "memory");
    return ret;
}

static inline unsigned int cstr_len(const char *s) {
    unsigned int n = 0u;
    while (s && s[n] != '\0') {
        ++n;
    }
    return n;
}

#endif
