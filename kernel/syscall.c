#include "kernel.h"

#define SYSCALL_WRITE_MAX 1024u
#define SYSCALL_RET_KERNEL_MAGIC ((int)0x534C4F50)

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
        console_putchar(CONSOLE_VGA, buf[i]);
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
    return task_current_pid();
}

static void ksys_exit(int code) {
    int pid = task_current_pid();
    if (pid > 0) {
        (void)proc_exit(pid, code);
    }
    console_printf(CONSOLE_BOTH, "process %d exited with code %d\n", pid, code);
}

int syscall_entry(u32 num, u32 a1, u32 a2, u32 a3, u32 a4, u32 a5) {
    int ret = -1;
    (void)a4;
    (void)a5;

    switch (num) {
        case SYS_READ:
            ret = ksys_read((int)a1, (char *)a2, a3);
            break;
        case SYS_WRITE:
            ret = ksys_write((int)a1, (const char *)a2, a3);
            break;
        case SYS_OPEN:
            ret = ksys_open((const char *)a1, a2);
            break;
        case SYS_CLOSE:
            ret = ksys_close((int)a1);
            break;
        case SYS_GETPID:
            ret = ksys_getpid();
            break;
        case SYS_EXIT:
            ksys_exit((int)a1);
            ret = 0;
            break;
        case SYS_RET_KERNEL:
            ret = SYSCALL_RET_KERNEL_MAGIC;
            break;
        default:
            ret = -1;
            break;
    }

    return ret;
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
