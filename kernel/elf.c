#include "kernel.h"

#define ELF_USER_IMAGE_MAX (128u * 1024u)
#define ELF_USER_IMAGE_BASE 0x00300000u
#define ELF_MAGIC0 0x7Fu
#define ELF_MAGIC1 'E'
#define ELF_MAGIC2 'L'
#define ELF_MAGIC3 'F'
#define ELFCLASS32 1u
#define ELFDATA2LSB 1u
#define EV_CURRENT 1u
#define ET_EXEC 2u
#define EM_386 3u
#define PT_LOAD 1u
#define ELF_PROC_SLOTS 8u
#define ELF_STACK_SIZE 4096u
#define ELF_ARG_MAX 16u

struct elf32_ehdr {
    u8 e_ident[16];
    u16 e_type;
    u16 e_machine;
    u32 e_version;
    u32 e_entry;
    u32 e_phoff;
    u32 e_shoff;
    u32 e_flags;
    u16 e_ehsize;
    u16 e_phentsize;
    u16 e_phnum;
    u16 e_shentsize;
    u16 e_shnum;
    u16 e_shstrndx;
} __attribute__((packed));

struct elf32_phdr {
    u32 p_type;
    u32 p_offset;
    u32 p_vaddr;
    u32 p_paddr;
    u32 p_filesz;
    u32 p_memsz;
    u32 p_flags;
    u32 p_align;
} __attribute__((packed));

static u8 user_stacks[ELF_PROC_SLOTS][ELF_STACK_SIZE];
static int slot_owner_pid[ELF_PROC_SLOTS];
static int slots_initialized = 0;

extern volatile u32 ring3_current_pid;

static void elf_slots_init_once(void) {
    u32 i;
    if (slots_initialized) {
        return;
    }
    for (i = 0; i < ELF_PROC_SLOTS; ++i) {
        slot_owner_pid[i] = -1;
    }
    slots_initialized = 1;
}

static int elf_slot_for_pid(int pid) {
    u32 i;

    elf_slots_init_once();

    for (i = 0; i < ELF_PROC_SLOTS; ++i) {
        if (slot_owner_pid[i] == pid) {
            return (int)i;
        }
    }

    for (i = 0; i < ELF_PROC_SLOTS; ++i) {
        if (slot_owner_pid[i] < 0 || proc_state_of(slot_owner_pid[i]) == PROC_UNUSED) {
            slot_owner_pid[i] = pid;
            return (int)i;
        }
    }

    return -1;
}

static int elf_validate_header(const struct elf32_ehdr *eh, u32 size, enum console_target target) {
    if (size < sizeof(struct elf32_ehdr)) {
        console_print(target, "run: ELF too small\n");
        return 0;
    }
    if (eh->e_ident[0] != ELF_MAGIC0 || eh->e_ident[1] != ELF_MAGIC1 ||
        eh->e_ident[2] != ELF_MAGIC2 || eh->e_ident[3] != ELF_MAGIC3) {
        console_print(target, "run: bad ELF magic\n");
        return 0;
    }
    if (eh->e_ident[4] != ELFCLASS32 || eh->e_ident[5] != ELFDATA2LSB || eh->e_ident[6] != EV_CURRENT) {
        console_print(target, "run: unsupported ELF class/data/version\n");
        return 0;
    }
    if (eh->e_type != ET_EXEC || eh->e_machine != EM_386 || eh->e_version != EV_CURRENT) {
        console_print(target, "run: unsupported ELF type/machine/version\n");
        return 0;
    }
    if (eh->e_phentsize != sizeof(struct elf32_phdr) || eh->e_phnum == 0u) {
        console_print(target, "run: missing/invalid program headers\n");
        return 0;
    }
    if (eh->e_phoff + ((u32)eh->e_phnum * (u32)eh->e_phentsize) > size) {
        console_print(target, "run: ELF headers out of range\n");
        return 0;
    }
    return 1;
}

int elf_load_from_vfs(struct vfs_node *cwd, const char *path, int pid, struct proc_image *out_image, enum console_target target) {
    const char *data;
    u32 size;
    const struct elf32_ehdr *eh;
    const struct elf32_phdr *ph;
    u32 i;
    u32 min_vaddr = 0xFFFFFFFFu;
    u32 max_end = 0u;
    int load_count = 0;
    int slot;

    if (!out_image || pid < 0 || !vfs_read_file(cwd, path, &data, &size) || !data) {
        console_print(target, "run: file not found\n");
        return 0;
    }

    slot = elf_slot_for_pid(pid);
    if (slot < 0) {
        console_print(target, "run: no free process image slots\n");
        return 0;
    }

    eh = (const struct elf32_ehdr *)data;
    if (!elf_validate_header(eh, size, target)) {
        return 0;
    }

    ph = (const struct elf32_phdr *)(data + eh->e_phoff);
    for (i = 0; i < eh->e_phnum; ++i) {
        const struct elf32_phdr *p = &ph[i];
        if (p->p_type != PT_LOAD) {
            continue;
        }
        if (p->p_memsz < p->p_filesz || p->p_offset + p->p_filesz > size) {
            console_print(target, "run: invalid PT_LOAD segment\n");
            return 0;
        }
        if (p->p_vaddr < min_vaddr) {
            min_vaddr = p->p_vaddr;
        }
        if (p->p_vaddr + p->p_memsz > max_end) {
            max_end = p->p_vaddr + p->p_memsz;
        }
        ++load_count;
    }

    if (load_count == 0 || min_vaddr >= max_end) {
        console_print(target, "run: no loadable segments\n");
        return 0;
    }

    if (min_vaddr < ELF_USER_IMAGE_BASE || max_end > (ELF_USER_IMAGE_BASE + ELF_USER_IMAGE_MAX)) {
        console_print(target, "run: image outside reserved user range\n");
        return 0;
    }

    if ((max_end - min_vaddr) > ELF_USER_IMAGE_MAX) {
        console_print(target, "run: image too large for user buffer\n");
        return 0;
    }

    mem_zero((void *)ELF_USER_IMAGE_BASE, ELF_USER_IMAGE_MAX);

    for (i = 0; i < eh->e_phnum; ++i) {
        const struct elf32_phdr *p = &ph[i];
        u32 j;
        if (p->p_type != PT_LOAD) {
            continue;
        }
        for (j = 0; j < p->p_filesz; ++j) {
            ((u8 *)p->p_vaddr)[j] = (u8)data[p->p_offset + j];
        }
    }

    out_image->loaded = 1;
    out_image->image_base = min_vaddr;
    out_image->image_size = (max_end - min_vaddr);
    out_image->entry = eh->e_entry;
    out_image->user_stack_base = (u32)&user_stacks[slot][0];
    out_image->user_stack_size = ELF_STACK_SIZE;
    return 1;
}

int elf_execute_image(const struct proc_image *image, int pid, int argc, char **argv, int *ret_value, enum console_target target) {
    u32 user_sp;
    u32 argv_user[ELF_ARG_MAX];
    u32 argv_table_addr;
    int i;
    int rc;

    if (!image || !image->loaded || pid < 0) {
        console_print(target, "run: no loaded image\n");
        return 0;
    }
    if (image->entry < image->image_base || image->entry >= (image->image_base + image->image_size)) {
        console_print(target, "run: entry outside loaded image\n");
        return 0;
    }

    if (image->user_stack_size < 64u) {
        console_print(target, "run: invalid user stack\n");
        return 0;
    }

    if (argc < 0) {
        argc = 0;
    }
    if ((u32)argc > ELF_ARG_MAX) {
        argc = (int)ELF_ARG_MAX;
    }

    user_sp = image->user_stack_base + image->user_stack_size;
    for (i = argc - 1; i >= 0; --i) {
        const char *src = (argv && argv[i]) ? argv[i] : "";
        u32 len = str_len(src) + 1u;
        u32 j;
        if (len > (image->user_stack_size - 64u)) {
            console_print(target, "run: argument too long\n");
            return 0;
        }
        if (user_sp < (image->user_stack_base + len + 16u)) {
            console_print(target, "run: arguments exceed user stack\n");
            return 0;
        }
        user_sp -= len;
        for (j = 0; j < len; ++j) {
            ((char *)user_sp)[j] = src[j];
        }
        argv_user[i] = user_sp;
    }

    user_sp &= ~3u;
    if (user_sp < (image->user_stack_base + ((u32)(argc + 1) * 4u) + 8u)) {
        console_print(target, "run: argument table exceeds user stack\n");
        return 0;
    }

    user_sp -= ((u32)(argc + 1) * 4u);
    argv_table_addr = user_sp;
    for (i = 0; i < argc; ++i) {
        ((u32 *)argv_table_addr)[i] = argv_user[i];
    }
    ((u32 *)argv_table_addr)[argc] = 0u;

    if (user_sp < (image->user_stack_base + 8u)) {
        console_print(target, "run: argument header exceeds user stack\n");
        return 0;
    }
    user_sp -= 8u;
    ((u32 *)user_sp)[0] = (u32)argc;
    ((u32 *)user_sp)[1] = argv_table_addr;

    ring3_current_pid = (u32)pid;
    rc = enter_user_mode(image->entry, user_sp);
    ring3_current_pid = 0u;
    if (ret_value) {
        *ret_value = rc;
    }
    return 1;
}
