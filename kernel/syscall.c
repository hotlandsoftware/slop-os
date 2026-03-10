#include "kernel.h"

#define SYSCALL_WRITE_MAX 1024u

static int ksys_read(int fd, char *buf, u32 len) {
    u32 i;
    char c;

    if (fd != 0 || !buf) {
        return -1;
    }

    for (i = 0; i < len; ++i) {
        while (!keyboard_try_read_char(&c)) {
            cpu_relax_wait();
        }
        buf[i] = c;
        if (c == '\n') {
            return (int)(i + 1u);
        }
    }

    return (int)len;
}

static int ksys_write(int fd, const char *buf, u32 len) {
    u32 i;

    if ((fd != 1 && fd != 2) || !buf) {
        return -1;
    }

    if (len > SYSCALL_WRITE_MAX) {
        len = SYSCALL_WRITE_MAX;
    }

    for (i = 0; i < len; ++i) {
        console_putchar(CONSOLE_BOTH, buf[i]);
    }

    return (int)len;
}

static int ksys_open(const char *path, u32 flags) {
    (void)path;
    (void)flags;
    return -1;
}

static int ksys_close(int fd) {
    (void)fd;
    return -1;
}

static int ksys_getpid(void) {
    return 1;
}

static void ksys_exit(int code) {
    console_printf(CONSOLE_BOTH, "process exited with code %d\n", code);
}

void syscall_dispatch(struct interrupt_frame *frame) {
    int ret = -1;

    switch (frame->eax) {
        case SYS_READ:
            ret = ksys_read((int)frame->ebx, (char *)frame->ecx, frame->edx);
            break;
        case SYS_WRITE:
            ret = ksys_write((int)frame->ebx, (const char *)frame->ecx, frame->edx);
            break;
        case SYS_OPEN:
            ret = ksys_open((const char *)frame->ebx, frame->ecx);
            break;
        case SYS_CLOSE:
            ret = ksys_close((int)frame->ebx);
            break;
        case SYS_GETPID:
            ret = ksys_getpid();
            break;
        case SYS_EXIT:
            ksys_exit((int)frame->ebx);
            ret = 0;
            break;
        default:
            ret = -1;
            break;
    }

    frame->eax = (u32)ret;
}

int sys_write(int fd, const char *buf, u32 len) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_WRITE), "b"(fd), "c"(buf), "d"(len)
        : "memory");
    return ret;
}

int sys_read(int fd, char *buf, u32 len) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_READ), "b"(fd), "c"(buf), "d"(len)
        : "memory");
    return ret;
}

int sys_open(const char *path, u32 flags) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_OPEN), "b"(path), "c"(flags)
        : "memory");
    return ret;
}

int sys_close(int fd) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_CLOSE), "b"(fd)
        : "memory");
    return ret;
}

int sys_getpid(void) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_GETPID)
        : "memory");
    return ret;
}

void sys_exit(int code) {
    __asm__ volatile (
        "int $0x80"
        :
        : "a"(SYS_EXIT), "b"(code)
        : "memory");
}
