#include "kernel.h"

#define SYSCALL_WRITE_MAX 1024u
#define ERR_BADF (-9)
#define ERR_AGAIN (-11)
#define ERR_FAULT (-14)
#define ERR_INVAL (-22)
#define ERR_NFILE (-24)

extern volatile u32 ring3_active;
extern volatile u32 ring3_current_pid;

struct fd_entry {
    int used;
    int pid;
    const char *data;
    u32 size;
    u32 offset;
};

static struct fd_entry fd_table[16];
static struct vfs_node *user_cwd = (struct vfs_node *)0;
static const struct multiboot_info *user_boot_mbi = (const struct multiboot_info *)0;
static u32 user_boot_magic = 0u;

void syscall_save_yield_context(const u32 *frame_base) {
    struct proc_image image;
    int pid;

    if (!frame_base) {
        return;
    }

    pid = (int)ring3_current_pid;
    if (pid <= 0 || !proc_get_image(pid, &image) || !image.loaded) {
        return;
    }

    image.context.edi = frame_base[0];
    image.context.esi = frame_base[1];
    image.context.ebp = frame_base[2];
    image.context.esp_dummy = frame_base[3];
    image.context.ebx = frame_base[4];
    image.context.edx = frame_base[5];
    image.context.ecx = frame_base[6];
    image.context.eax = 0u;
    image.context.eip = frame_base[8];
    image.context.cs = frame_base[9];
    image.context.eflags = frame_base[10];
    image.context.user_esp = frame_base[11];
    image.context.user_ss = frame_base[12];
    image.context_valid = 1;
    (void)proc_bind_image(pid, &image);
}

void syscall_set_user_cwd(struct vfs_node *cwd) {
    user_cwd = cwd;
}

void syscall_set_bootinfo(const struct multiboot_info *mbi, u32 magic) {
    user_boot_mbi = mbi;
    user_boot_magic = magic;
}

static int range_contains(u32 base, u32 size, u32 ptr, u32 len) {
    u32 end;
    if (size == 0u) {
        return 0;
    }
    if (ptr < base) {
        return 0;
    }
    if (len == 0u) {
        return ptr < (base + size);
    }
    end = ptr + len;
    if (end < ptr) {
        return 0;
    }
    return end <= (base + size);
}

static int validate_user_ptr(const void *ptr, u32 len) {
    struct proc_image image;
    u32 p = (u32)ptr;
    int pid;

    if (ring3_active == 0u) {
        return 1;
    }

    pid = (int)ring3_current_pid;
    if (pid <= 0 || !proc_get_image(pid, &image) || !image.loaded) {
        return 0;
    }

    if (range_contains(image.image_base, image.image_size, p, len)) {
        return 1;
    }
    if (range_contains(image.user_stack_base, image.user_stack_size, p, len)) {
        return 1;
    }
    return 0;
}

static int copy_user_cstr(const char *user_ptr, char *dst, u32 dst_size) {
    u32 i = 0;
    if (!user_ptr || !dst || dst_size == 0u) {
        return 0;
    }

    for (i = 0; i < (dst_size - 1u); ++i) {
        char c;
        if (!validate_user_ptr(user_ptr + i, 1u)) {
            return 0;
        }
        c = user_ptr[i];
        dst[i] = c;
        if (c == '\0') {
            return 1;
        }
    }

    dst[dst_size - 1u] = '\0';
    return 1;
}

static int fd_alloc_slot(void) {
    u32 i;
    for (i = 3u; i < (sizeof(fd_table) / sizeof(fd_table[0])); ++i) {
        if (!fd_table[i].used) {
            return (int)i;
        }
    }
    return -1;
}

static int fd_lookup(int fd, int pid) {
    if (fd < 3 || (u32)fd >= (sizeof(fd_table) / sizeof(fd_table[0]))) {
        return -1;
    }
    if (!fd_table[fd].used || fd_table[fd].pid != pid) {
        return -1;
    }
    return fd;
}

static int ksys_read(int fd, char *buf, u32 len) {
    u32 i;
    char c;
    int pid = (int)ring3_current_pid;
    int slot;

    if (fd != 0 || !buf) {
        if (fd != 0 || !buf) {
            if (fd == 0) {
                return ERR_FAULT;
            }
        }
    }

    if (fd == 0) {
        if (!validate_user_ptr(buf, len == 0u ? 1u : len)) {
            return ERR_FAULT;
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

    if (!validate_user_ptr(buf, len == 0u ? 1u : len)) {
        return ERR_FAULT;
    }

    slot = fd_lookup(fd, pid);
    if (slot < 0) {
        return ERR_BADF;
    }

    if (fd_table[slot].offset >= fd_table[slot].size) {
        return 0;
    }

    i = 0;
    while (i < len && fd_table[slot].offset < fd_table[slot].size) {
        buf[i++] = fd_table[slot].data[fd_table[slot].offset++];
    }

    return (int)i;
}

static int ksys_write(int fd, const char *buf, u32 len) {
    u32 i;

    if ((fd != 1 && fd != 2) || !buf) {
        return ERR_BADF;
    }

    if (len > SYSCALL_WRITE_MAX) {
        len = SYSCALL_WRITE_MAX;
    }

    if (!validate_user_ptr(buf, len == 0u ? 1u : len)) {
        return ERR_FAULT;
    }

    for (i = 0; i < len; ++i) {
        console_putchar(CONSOLE_VGA, buf[i]);
    }

    return (int)len;
}

static int ksys_open(const char *path, u32 flags) {
    char kpath[128];
    const char *data;
    u32 size = 0;
    int pid = (int)ring3_current_pid;
    int slot;

    if (!path) {
        return ERR_INVAL;
    }
    if (!copy_user_cstr(path, kpath, sizeof(kpath))) {
        return ERR_FAULT;
    }

    if ((flags & 0x40u) != 0u) {
        if (!vfs_touch(user_cwd ? user_cwd : vfs_root(), kpath)) {
            return ERR_INVAL;
        }
    }

    if (!vfs_read_file(user_cwd ? user_cwd : vfs_root(), kpath, &data, &size)) {
        return ERR_INVAL;
    }

    slot = fd_alloc_slot();
    if (slot < 0) {
        return ERR_NFILE;
    }

    fd_table[slot].used = 1;
    fd_table[slot].pid = pid;
    fd_table[slot].data = data;
    fd_table[slot].size = size;
    fd_table[slot].offset = 0;
    return slot;
}

static int ksys_close(int fd) {
    int pid = (int)ring3_current_pid;
    int slot = fd_lookup(fd, pid);
    if (slot < 0) {
        return ERR_BADF;
    }
    fd_table[slot].used = 0;
    fd_table[slot].pid = -1;
    fd_table[slot].data = (const char *)0;
    fd_table[slot].size = 0u;
    fd_table[slot].offset = 0u;
    return 0;
}

static int ksys_getpid(void) {
    if (ring3_active != 0u && ring3_current_pid > 0u) {
        return (int)ring3_current_pid;
    }
    return task_current_pid();
}

static int ksys_mkdir(const char *path) {
    char kpath[128];
    if (!path) {
        return ERR_INVAL;
    }
    if (!copy_user_cstr(path, kpath, sizeof(kpath))) {
        return ERR_FAULT;
    }
    if (!vfs_make_dir(user_cwd ? user_cwd : vfs_root(), kpath)) {
        return ERR_INVAL;
    }
    return 0;
}

static int ksys_getcwd(char *buf, u32 len) {
    char path[128];
    u32 i;
    u32 n;
    if (!buf || len == 0u) {
        return ERR_INVAL;
    }
    if (!validate_user_ptr(buf, len)) {
        return ERR_FAULT;
    }
    vfs_get_cwd_path(user_cwd ? user_cwd : vfs_root(), path, sizeof(path));
    n = str_len(path);
    if (n + 1u > len) {
        return ERR_INVAL;
    }
    for (i = 0; i <= n; ++i) {
        buf[i] = path[i];
    }
    return (int)n;
}

static int ksys_list(const char *path) {
    char kpath[128];
    if (!path || (u32)path == 0u) {
        vfs_list(user_cwd ? user_cwd : vfs_root(), (const char *)0, CONSOLE_VGA);
        return 0;
    }
    if (!copy_user_cstr(path, kpath, sizeof(kpath))) {
        return ERR_FAULT;
    }
    vfs_list(user_cwd ? user_cwd : vfs_root(), kpath, CONSOLE_VGA);
    return 0;
}

static int ksys_systeminfo(void) {
    if (!user_boot_mbi || user_boot_magic != MULTIBOOT_MAGIC) {
        console_print(CONSOLE_VGA, "systeminfo: boot info unavailable\n");
        return ERR_INVAL;
    }
    print_systeminfo(user_boot_mbi, user_boot_magic);
    return 0;
}

static int ksys_ps(void) {
    task_list(CONSOLE_VGA);
    return 0;
}

static int ksys_meminfo(struct mem_info *info) {
    u32 total_kib = 0u;
    u32 kernel_bytes = (u32)(&_kernel_end - &_kernel_start);
    u32 used_bytes = kernel_bytes;
    u32 free_bytes = 0u;

    if (!info || !validate_user_ptr(info, sizeof(*info))) {
        return ERR_FAULT;
    }

    if (user_boot_mbi && user_boot_magic == MULTIBOOT_MAGIC && (user_boot_mbi->flags & 0x1u)) {
        total_kib = user_boot_mbi->mem_lower + user_boot_mbi->mem_upper;
    }

    if (heap_is_ready()) {
        used_bytes += heap_bytes_used();
        free_bytes = heap_bytes_free();
    } else if (total_kib > 1024u) {
        u32 total_bytes = total_kib * 1024u;
        if (total_bytes > kernel_bytes) {
            free_bytes = total_bytes - kernel_bytes;
        }
    }

    info->mem_total_kib = total_kib;
    info->mem_used_kib = (used_bytes + 1023u) / 1024u;
    if (total_kib > info->mem_used_kib) {
        info->mem_free_kib = total_kib - info->mem_used_kib;
    } else {
        info->mem_free_kib = 0u;
    }
    info->mem_shared_kib = 0u;
    info->mem_buff_cache_kib = 0u;
    info->mem_available_kib = free_bytes / 1024u;
    if (info->mem_available_kib > info->mem_free_kib) {
        info->mem_available_kib = info->mem_free_kib;
    }
    info->swap_total_kib = 0u;
    info->swap_used_kib = 0u;
    info->swap_free_kib = 0u;
    return 0;
}

static int ksys_ipc_send(struct ipc_message *msg) {
    struct ipc_message kmsg;
    int src_pid = (int)ring3_current_pid;

    if (!msg || !validate_user_ptr(msg, sizeof(*msg))) {
        return ERR_FAULT;
    }
    kmsg = *msg;
    if (!ipc_send(src_pid, kmsg.dst_pid, &kmsg)) {
        return ERR_AGAIN;
    }
    return 0;
}

static int ksys_ipc_recv(struct ipc_message *msg) {
    struct ipc_message kmsg;
    int dst_pid = (int)ring3_current_pid;

    if (!msg || !validate_user_ptr(msg, sizeof(*msg))) {
        return ERR_FAULT;
    }
    if (!ipc_recv(dst_pid, &kmsg)) {
        return ERR_AGAIN;
    }
    *msg = kmsg;
    return 0;
}

static int ksys_ipc_reply(struct ipc_message *msg) {
    struct ipc_message kmsg;
    int src_pid = (int)ring3_current_pid;

    if (!msg || !validate_user_ptr(msg, sizeof(*msg))) {
        return ERR_FAULT;
    }
    kmsg = *msg;
    if (!ipc_reply(src_pid, kmsg.dst_pid, &kmsg)) {
        return ERR_AGAIN;
    }
    return 0;
}

static int ksys_yield(void) {
    return 0;
}

static void ksys_exit(int code) {
    int pid = task_current_pid();
    if (pid > 0) {
        (void)proc_exit(pid, code);
    }
    console_print(CONSOLE_BOTH, "process ");
    if (pid < 0) {
        console_print(CONSOLE_BOTH, "-");
        console_print_u32_dec(CONSOLE_BOTH, (u32)(-pid));
    } else {
        console_print_u32_dec(CONSOLE_BOTH, (u32)pid);
    }
    console_print(CONSOLE_BOTH, " exited with code ");
    if (code < 0) {
        console_print(CONSOLE_BOTH, "-");
        console_print_u32_dec(CONSOLE_BOTH, (u32)(-code));
    } else {
        console_print_u32_dec(CONSOLE_BOTH, (u32)code);
    }
    console_putchar(CONSOLE_BOTH, '\n');
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
        case SYS_MKDIR:
            ret = ksys_mkdir((const char *)a1);
            break;
        case SYS_GETCWD:
            ret = ksys_getcwd((char *)a1, a2);
            break;
        case SYS_EXIT:
            ksys_exit((int)a1);
            ret = 0;
            break;
        case SYS_LIST:
            ret = ksys_list((const char *)a1);
            break;
        case SYS_SYSTEMINFO:
            ret = ksys_systeminfo();
            break;
        case SYS_PS:
            ret = ksys_ps();
            break;
        case SYS_MEMINFO:
            ret = ksys_meminfo((struct mem_info *)a1);
            break;
        case SYS_IPC_SEND:
            ret = ksys_ipc_send((struct ipc_message *)a1);
            break;
        case SYS_IPC_RECV:
            ret = ksys_ipc_recv((struct ipc_message *)a1);
            break;
        case SYS_IPC_REPLY:
            ret = ksys_ipc_reply((struct ipc_message *)a1);
            break;
        case SYS_YIELD:
            ret = ksys_yield();
            break;
        case SYS_RET_KERNEL:
            ret = 0;
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

int sys_meminfo(struct mem_info *info) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_MEMINFO), "b"(info)
        : "memory");
    return ret;
}

int sys_ipc_send(struct ipc_message *msg) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_IPC_SEND), "b"(msg)
        : "memory");
    return ret;
}

int sys_ipc_recv(struct ipc_message *msg) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_IPC_RECV), "b"(msg)
        : "memory");
    return ret;
}

int sys_ipc_reply(struct ipc_message *msg) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_IPC_REPLY), "b"(msg)
        : "memory");
    return ret;
}

int sys_yield(void) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_YIELD)
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
